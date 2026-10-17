#include "Layout.h"

#include <ReceiptLedger/engine/receipt/Interpreter.h>
#include <regex>
#include <set>

namespace ReceiptLedger::Engine::Receipt {

Result Interpreter::interpret(const Ocr::Result& source, const std::string& pageId) {
    using Internal::Receipt::Layout;
    using Internal::Receipt::Row;

    Result result;
    result.id = pageId + "-receipt-1";
    result.reviewReasons = {"one candidate receipt per page; page association requires review",
                            "heuristic interpretation requires source review; financial match does not approve OCR"};

    const auto rows = Layout::rows(source);
    const std::regex number(R"(([+-]?[0-9]+[,.][0-9]+))");
    const std::regex date(R"(\b([0-9]{2}[./][0-9]{2}[./][0-9]{4})\b)");
    const std::regex time(R"(\b([0-9]{2}:[0-9]{2}(?::[0-9]{2})?)\b)");
    const std::regex multiplication(
        R"(^\s*([+-]?[0-9]+(?:[,.][0-9]+)?)\s*(kg|KG|l|L)?\s*[xX*]\s*([+-]?[0-9]+[,.][0-9]+))");
    const std::regex leadingQuantity(R"(^\s*([+-]?[0-9]+(?:[,.][0-9]+)?)\s+([\s\S]+))");
    const std::regex currency(R"((EUR|[Ee]uro|€))");
    const std::regex taxRow(
        R"(^\s*([A-Z0-9])?\s*([0-9]+(?:[,.][0-9]+)?)\s*%\s+([0-9]+[,.][0-9]+)\s+([0-9]+[,.][0-9]+)\s+([0-9]+[,.][0-9]+)\s*$)");
    const std::regex taxOn(
        R"(BTW\s*([0-9]+[,.][0-9]+)\s*%\s*(?:OP|op)\s*([0-9]+[,.][0-9]+)\s*[^0-9]*=\s*([0-9]+[,.][0-9]+))");
    bool inItems = false;
    bool ended = false;
    bool priceColumns = false;
    bool articleNumbers = false;
    int descriptionLeft = -1;
    int quantityLeft = -1;
    int quantityRight = -1;
    bool totalColumns = false;
    std::optional<Row> pendingDescription;
    std::optional<Row> pendingMultiplication;
    const auto money = [](const std::string& text) { return Decimal::read(text).cents(); };

    const auto merge = [](Row left, const Row& right) {
        left.text += "\n" + right.text;
        left.regions.insert(left.regions.end(), right.regions.begin(), right.regions.end());
        left.bottom = right.bottom;
        left.uncertain |= right.uncertain;
        return left;
    };

    const auto fieldEvidence = [](const Row& row, const Ocr::Region& field) {
        Row evidence = row;
        evidence.text = Layout::trim(field.text);
        evidence.regions.insert(evidence.regions.begin(), field.id);
        return evidence;
    };

    const auto applyReadings = [&](Item& item, const Row& row) {
        for (const auto& field : row.fields) {
            const auto text = Layout::trim(field.text);
            if (!field.confidence || *field.confidence < 60.0F)
                continue;

            if (field.purpose == "amount" && std::regex_match(text, std::regex(R"([+-]?[0-9]+[,.][0-9]{2})"))) {
                const auto previous = item.printed.raw;
                std::string originalNumber;
                for (auto match = std::sregex_iterator(previous.begin(), previous.end(), number);
                     match != std::sregex_iterator(); ++match)
                    originalNumber = match->str();

                const auto separator = originalNumber.find_first_of(",.");
                const bool printedCentPrecision =
                    separator != std::string::npos && originalNumber.size() - separator - 1 == 2;
                if (item.printed.value && printedCentPrecision && *item.printed.value != money(text)) {
                    item.printed.alternatives.push_back(text);
                    item.reviewReasons.push_back(
                        "conflicting column amount reread; original printed reading retained for review");
                    continue;
                }

                item.printed =
                    Layout::printed(money(text), fieldEvidence(row, field), "aligned-amount-column-image-reread");
                item.printed.alternatives.push_back(previous);
                item.reviewReasons.push_back("amount reread in observed column; original full-page OCR retained");
            }
            if (field.purpose == "unit-price" && priceColumns &&
                std::regex_match(text, std::regex(R"([+-]?[0-9]+[,.][0-9]{1,6})"))) {
                const auto previous = item.unitPrice.raw;
                if (item.unitPrice.value && item.unitPrice.value->text('.') != Decimal::read(text).text('.')) {
                    item.unitPrice.alternatives.push_back(text);
                    item.reviewReasons.push_back(
                        "conflicting unit-price reread or precision; original reading retained");
                    continue;
                }

                item.unitPrice = Layout::printed(Decimal::read(text), fieldEvidence(row, field),
                                                 "aligned-unit-price-column-image-reread");
                item.unitPrice.alternatives.push_back(previous);
            }
            if (field.purpose == "marker" && std::regex_match(text, std::regex(R"([A-Z0-9])")))
                item.marker = Layout::printed(text, fieldEvidence(row, field), "separate-printed-code-column");
        }
    };

    const auto applyMultiplication = [&](Item& item, const Row& row) {
        Row evidence = row;
        bool reread = false;
        for (const auto& field : row.fields) {
            if (field.purpose == "multiplication" && field.confidence && *field.confidence >= 60.0F &&
                std::regex_search(field.text, multiplication)) {
                evidence = fieldEvidence(row, field);
                reread = true;
            }
        }
        std::smatch parsed;
        if (std::regex_search(evidence.text, parsed, multiplication)) {
            item.quantity =
                Layout::printed(Decimal::read(parsed[1].str()), evidence, "explicit-multiplication-quantity");
            item.unitPrice =
                Layout::printed(Decimal::read(parsed[3].str()), evidence, "explicit-multiplication-unit-price");
            if (parsed[2].matched)
                item.unit = Layout::printed(parsed[2].str(), evidence, "explicit-quantity-unit");

            if (reread) {
                item.quantity.rule = "multiplication-column-image-reread";
                item.unitPrice.rule = item.quantity.rule;
                item.quantity.alternatives.push_back(row.text);
                item.unitPrice.alternatives.push_back(row.text);
                item.reviewReasons.push_back("quantity and price reread before currency column; original OCR retained");
            }
        } else if (std::regex_search(row.text, parsed, std::regex(R"(^\s*([+-]?[0-9]+(?:[,.][0-9]+)?)\s*[xX*])")))
            item.quantity =
                Layout::printed(Decimal::read(parsed[1].str()), row, "multiplication-quantity-price-unreadable");
    };

    bool explicitVatColumn = false;
    bool nutritionLabel = false;
    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        Row row = rows[rowIndex];
        for (const auto& field : row.fields) {
            std::smatch parsed;
            if (field.purpose == "tax-summary" && field.confidence && *field.confidence >= 60.0F &&
                std::regex_search(field.text, taxOn)) {
                row.text = field.text;
                row.regions.insert(row.regions.begin(), field.id);
            }
            const std::regex descriptionLine(R"((?:^|\n)[^a-zA-Z0-9+-]*([0-9]+)\s+([^\r\n]+))");
            // Keep structurally identified description candidates even when OCR is uncertain, as with original page text.
            if (field.purpose == "description" && field.confidence && *field.confidence > 0.0F &&
                std::regex_search(field.text, parsed, descriptionLine)) {
                row.text = parsed[1].str() + " " + parsed[2].str();
                row.uncertain = true;
                row.regions.insert(row.regions.begin(), field.id);
            }
        }

        const auto upper = Layout::upper(row.text);
        const bool wasEnded = ended;
        nutritionLabel |= upper.find("NUTRI") != std::string::npos;
        explicitVatColumn |= upper.find("OMSCHRIJVING") != std::string::npos && upper.find("BTW") != std::string::npos;
        std::smatch match;
        for (const auto& word : row.words) {
            if (Layout::upper(word.text).find("BENAMING") != std::string::npos) {
                int previous = 0;
                for (const auto& header : row.words) {
                    if (header.right < word.left)
                        previous = std::max(previous, header.right);
                }
                descriptionLeft =
                    previous ? (previous + word.left) / 2 : std::max(0, word.left - (word.bottom - word.top) / 2);
            }
            if (Layout::upper(word.text).find("HOEV") != std::string::npos) {
                quantityLeft = word.left - 25;
                quantityRight = word.right + 25;
            }
        }

        if (upper.find("KLANTTICKET") != std::string::npos || upper.find("POI:") != std::string::npos ||
            upper.find("SPAARACTIE") != std::string::npos || upper.find("BETALING") != std::string::npos ||
            upper.find("BETAALD:") != std::string::npos || upper.find("BANCONTACT") != std::string::npos ||
            upper.find("TE BETALEN") != std::string::npos || upper.find("TOTAAL GOEDEREN") != std::string::npos)
            ended = true;

        if (upper.find("TOT") != std::string::npos && upper.find("BEDRAG") != std::string::npos &&
            upper.find("BTW") != std::string::npos)
            totalColumns = true;

        if (!result.date.value && std::regex_search(row.text, match, date))
            result.date = Layout::printed(match[1].str(), row, "printed-date");

        if (!result.time.value && (std::regex_search(row.text, date) || upper.find("DATUM") != std::string::npos) &&
            std::regex_search(row.text, match, time))
            result.time = Layout::printed(match[1].str(), row, "printed-time");

        if (!result.currency.value && std::regex_search(row.text, currency))
            result.currency = Layout::printed(std::string("EUR"), row, "explicit-currency-marker");

        if (!result.store.value &&
            (upper == "GAMMA" || upper.find("ALDI") != std::string::npos ||
             upper.find("OKAY MEERHOUT") != std::string::npos || upper.find("AUTO 5") != std::string::npos ||
             upper.find("WWW.AUTO5.BE") != std::string::npos || upper.find("WWW.ELS-GARDEN.BE") != std::string::npos))
            result.store = Layout::printed(row.text, row, "merchant-text-marker");

        if (std::regex_match(row.text, match, taxRow)) {
            Tax tax;
            if (match[1].matched)
                tax.code = Layout::printed(match[1].str(), row, "tax-table-code");

            tax.rate = Layout::printed(Decimal::read(match[2].str()), row, "tax-table-rate");
            tax.net = Layout::printed(money(match[3].str()), row, "tax-table-net");
            tax.tax = Layout::printed(money(match[4].str()), row, "tax-table-tax");
            tax.gross = Layout::printed(money(match[5].str()), row, "tax-table-gross");
            for (const auto& field : row.fields) {
                const auto text = Layout::trim(field.text);
                if (!field.confidence || *field.confidence < 60.0F ||
                    !std::regex_match(text, std::regex(R"([0-9]+[,.][0-9]{2})")))
                    continue;

                auto* target = field.purpose == "tax-net"      ? &tax.net
                               : field.purpose == "tax-amount" ? &tax.tax
                               : field.purpose == "tax-gross"  ? &tax.gross
                                                               : nullptr;
                if (target) {
                    const auto previous = target->raw;
                    *target =
                        Layout::printed(money(text), fieldEvidence(row, field), "printed-VAT-table-cell-image-reread");
                    target->alternatives.push_back(previous);
                }
            }
            result.taxes.push_back(std::move(tax));
            continue;
        }

        if (std::regex_search(row.text, match, taxOn)) {
            Tax tax;
            tax.rate = Layout::printed(Decimal::read(match[1].str()), row, "tax-on-net-rate");
            tax.net = Layout::printed(money(match[2].str()), row, "tax-on-net-base");
            tax.tax = Layout::printed(money(match[3].str()), row, "tax-on-net-tax");
            result.taxes.push_back(std::move(tax));
            continue;
        }

        if ((upper.find("AANT") != std::string::npos && upper.find("SCHRIJ") != std::string::npos) ||
            (upper.find("OMSCHRIJVING") != std::string::npos && upper.find("PRIJS") != std::string::npos) ||
            upper.find("BENAMING") != std::string::npos ||
            (upper.find("ARTIKEL") != std::string::npos && upper.find("TOTAAL") != std::string::npos)) {
            inItems = true;
            priceColumns = true;
            articleNumbers = upper.find("BENAMING") != std::string::npos;
            continue;
        }

        std::vector<std::smatch> numbers;
        for (auto begin = std::sregex_iterator(row.text.begin(), row.text.end(), number);
             begin != std::sregex_iterator(); ++begin)
            numbers.push_back(*begin);

        const auto amountField = std::ranges::find_if(row.fields, [&](const auto& field) {
            return field.purpose == "amount" && field.confidence && *field.confidence >= 60.0F &&
                   std::regex_match(Layout::trim(field.text), std::regex(R"([+-]?[0-9]+[,.][0-9]{2})"));
        });
        const bool hasAmountField = amountField != row.fields.end();
        if (numbers.empty() && hasAmountField) {
            std::smatch parsed;
            if (std::regex_search(amountField->text, parsed, number))
                numbers.push_back(parsed);
        }
        if (totalColumns && numbers.size() >= 2 && upper.find("BEDRAG") == std::string::npos) {
            if (!result.total.value)
                result.total = Layout::printed(money(numbers.front().str()), row, "printed-total-bedrag-column");
            totalColumns = false;
            continue;
        }

        const auto startText = std::regex_replace(row.text, std::regex(R"(^[^a-zA-Z0-9+-]+)"), "");
        if (!ended && numbers.empty() && std::regex_match(startText, match, leadingQuantity) &&
            rowIndex + 1 < rows.size()) {
            const auto& next = rows[rowIndex + 1].text;
            if (std::distance(std::sregex_iterator(next.begin(), next.end(), number), std::sregex_iterator()) >= 2) {
                inItems = true;
                priceColumns = true;
                pendingDescription = row;
                pendingDescription->text = startText;
                continue;
            }
        }

        const std::regex unitTable(
            R"(^\S+\s+(.+?)\s+(Ton|kg|KG|l|L|st)\s+([+-]?[0-9]+(?:[,.][0-9]+)?)\s+([+-]?[0-9]+[,.][0-9]+)\s+([+-]?[0-9]+(?:[,.][0-9]+)?)\s+([0-9]{1,2})$)");
        const auto tableText = std::regex_replace(row.text, std::regex(R"([|:\[\]])"), " ");
        if (inItems && !ended && std::regex_match(tableText, match, unitTable)) {
            Item item;
            item.id = result.id + "-item-" + std::to_string(result.items.size() + 1);
            item.description = Layout::printed(match[1].str(), row, "unit-table-description");
            item.unit = Layout::printed(match[2].str(), row, "unit-table-unit");
            item.quantity = Layout::printed(Decimal::read(match[3].str()), row, "unit-table-quantity");
            item.unitPrice = Layout::printed(Decimal::read(match[4].str()), row, "unit-table-price");
            item.printed = Layout::printed(money(match[5].str()), row, "unit-table-amount");
            item.vatRate = Layout::printed(Decimal::read(match[6].str()), row, "unit-table-vat-rate");
            item.reviewReasons = {"heuristic table layout; verify source"};
            applyReadings(item, row);
            result.items.push_back(std::move(item));
            continue;
        }

        const bool totalLine = upper.find("TE BETALEN") != std::string::npos ||
                               upper.find("TOTAAL INCL") != std::string::npos ||
                               std::regex_search(upper, std::regex(R"(TOTAAL[^A-Z0-9]*EUR)")) ||
                               (upper.find("TOTAAL") != std::string::npos && upper.find("EUR") != std::string::npos) ||
                               std::regex_search(upper, std::regex(R"(^\s*[0-9]*\s*TOTAAL\s*:?)"));
        if (totalLine && !numbers.empty() && upper.find("TVA") == std::string::npos &&
            (upper.find("BTW") == std::string::npos || upper.find("INCL") != std::string::npos)) {
            if (result.total.value && *result.total.value != money(numbers.back().str()))
                result.reviewReasons.push_back("conflicting printed total candidate: " + row.text);

            if (!result.total.value &&
                (!wasEnded || std::regex_search(upper, std::regex(R"(TOTAAL[^A-Z0-9]*EUR)")) ||
                 upper.find("TE BETALEN") != std::string::npos || upper.find("INCL") != std::string::npos))
                result.total = Layout::printed(money(numbers.back().str()), row, "explicit-receipt-total");

            ended = true;
            pendingDescription.reset();
            pendingMultiplication.reset();
            continue;
        }

        if (upper.find("SUBTOTAAL") != std::string::npos && !numbers.empty()) {
            result.subtotal = Layout::printed(money(numbers.back().str()), row, "explicit-subtotal");
            continue;
        }

        if (upper.find("AFGEROND") != std::string::npos || upper.find("BETALING") != std::string::npos ||
            upper.find("BANCONTACT") != std::string::npos || upper.find("TERUG") != std::string::npos ||
            upper.find("WISSELGELD") != std::string::npos || upper.find("PINBETALING") != std::string::npos ||
            upper.find("BTW") != std::string::npos || upper.find("TVA") != std::string::npos ||
            upper.find("TOTAAL GOEDEREN") != std::string::npos || upper.find("TOTAAL BETAALD") != std::string::npos)
            continue;

        if (ended)
            continue;

        if (upper.find("PRIJS") != std::string::npos && upper.find("/KG") != std::string::npos) {
            pendingDescription = row;
            pendingDescription->text = Layout::trim(row.text.substr(0, upper.find("PRIJS")));
            inItems = true;
            continue;
        }

        if (std::regex_search(row.text, std::regex(R"(^\s*[0-9]+\s*[xX*])"))) {
            pendingMultiplication = row;
            inItems = true;
            if (!std::regex_search(row.text, multiplication))
                continue;
        }

        if (std::regex_search(row.text, match, multiplication)) {
            pendingMultiplication = row;
            inItems = true;
            if (upper.find('=') == std::string::npos)
                continue;
        }

        if (upper.find("ARTIKELKORTING") != std::string::npos && !result.items.empty() && !numbers.empty()) {
            auto& item = result.items.back();
            item.discount = Layout::printed(money(numbers.back().str()), row, "explicit-item-discount");
            continue;
        }

        if (row.text.empty() || upper == "ECO" || std::regex_search(upper, std::regex(R"(GEWICHTSART\s*IKELEN)")))
            continue;

        const bool correction = upper.find("KORTING") != std::string::npos ||
                                upper.find("COMMERCIAL OPERATION") != std::string::npos ||
                                upper.find("STATIEGELD") != std::string::npos;
        const bool hasLetters =
            std::ranges::any_of(row.text, [](unsigned char character) { return std::isalpha(character); });
        const bool separatedAmount = inItems && !priceColumns && std::ranges::any_of(row.words, [&](const auto& word) {
                                         return word.left > source.width * 65 / 100;
                                     });

        if (numbers.empty() && !separatedAmount) {
            if (inItems && hasLetters) {
                if (!pendingDescription && !result.items.empty() && !row.text.empty() &&
                    std::islower(static_cast<unsigned char>(row.text.front()))) {
                    auto& description = result.items.back().description;
                    *description.value += "\n" + row.text;
                    description.raw += "\n" + row.text;
                    description.regions.insert(description.regions.end(), row.regions.begin(), row.regions.end());
                    description.rule = "table-row-with-lowercase-continuation";
                    continue;
                }

                if (pendingDescription)
                    pendingDescription = merge(*pendingDescription, row);
                else
                    pendingDescription = row;
            }
            continue;
        }

        if (numbers.empty() && separatedAmount) {
            Item item;
            item.id = result.id + "-item-" + std::to_string(result.items.size() + 1);
            std::string description;
            for (const auto& word : row.words) {
                if (word.left < source.width * 65 / 100)
                    description += (description.empty() ? "" : " ") + word.text;
            }
            item.description = Layout::printed(description, row, "description-before-unreadable-amount-column");
            item.printed = {std::nullopt, "unknown", row.text, row.regions, "unreadable-amount-column"};
            if (pendingMultiplication)
                applyMultiplication(item, *pendingMultiplication);

            item.reviewReasons = {"amount not reliably readable; no numeric repair"};
            applyReadings(item, row);
            result.items.push_back(std::move(item));
            pendingMultiplication.reset();
            continue;
        }

        const auto& last = numbers.back();
        const auto suffix = static_cast<std::size_t>(last.position() + last.length()) <= row.text.size()
                                ? Layout::trim(row.text.substr(last.position() + last.length()))
                                : std::string{};
        const bool amountAtEnd = suffix.empty() || suffix == "€" || suffix == "EUR" || suffix == "euro" ||
                                 std::regex_match(suffix, std::regex(R"((?:€\s*)?[A-Z0-9])"));
        if ((!amountAtEnd && !separatedAmount && !hasAmountField &&
             !(pendingDescription && priceColumns && numbers.size() >= 2 && suffix.size() <= 4)) ||
            (!inItems && !pendingMultiplication))
            continue;

        Row evidence = pendingDescription ? merge(*pendingDescription, row) : row;
        Item item;
        item.id = result.id + (correction ? "-correction-" : "-item-") +
                  std::to_string((correction ? result.corrections.size() : result.items.size()) + 1);
        item.kind = correction ? (upper.find("STATIEGELD") != std::string::npos ? "deposit" : "discount") : "purchase";
        std::string description =
            pendingDescription ? pendingDescription->text : Layout::trim(row.text.substr(0, last.position()));
        if (priceColumns && !pendingDescription && numbers.size() >= 2)
            description = Layout::trim(row.text.substr(0, numbers[numbers.size() - 2].position()));

        const auto unparsedDescription = description;
        if (articleNumbers)
            description = std::regex_replace(description, std::regex(R"(^[A-Z]?[0-9]{3,}\s*)"), "");

        const std::regex trailingQuantity(R"(^(.*?)\s+([+-]?[0-9]+(?:[,.][0-9]+)?)(kg|KG|l|L)?$)");
        if (priceColumns && !pendingDescription && std::regex_match(description, match, trailingQuantity)) {
            item.quantity = Layout::printed(Decimal::read(match[2].str()), evidence, "quantity-before-price-columns");
            description = match[1].str();
            if (match[3].matched)
                item.unit = Layout::printed(match[3].str(), evidence, "printed-quantity-unit");

        } else if (std::regex_match(description, match, leadingQuantity)) {
            item.quantity = Layout::printed(Decimal::read(match[1].str()), evidence, "leading-table-quantity");
            description = match[2].str();
        }
        if (description.empty() && separatedAmount) {
            for (const auto& word : row.words) {
                if (word.left < source.width * 65 / 100)
                    description += (description.empty() ? "" : " ") + word.text;
            }
        }
        if (description.empty()) {
            result.reviewReasons.push_back("unassociated amount row: " + row.text);
            continue;
        }

        if (priceColumns && quantityLeft >= 0 && !row.words.empty()) {
            item.quantity = {};
            description = unparsedDescription;
            for (const auto& word : row.words) {
                if (word.left >= quantityLeft && word.left <= quantityRight &&
                    std::regex_search(word.text, match, std::regex(R"(^([+-]?[0-9]+(?:[,.][0-9]+)?))"))) {
                    item.quantity = Layout::printed(Decimal::read(match[1].str()), row, "quantity-column-geometry");
                    const auto at = description.rfind(word.text);
                    if (at != std::string::npos)
                        description = Layout::trim(description.substr(0, at));
                }
            }
        }
        if (separatedAmount) {
            description.clear();
            for (const auto& word : row.words) {
                if (word.left < source.width * 65 / 100)
                    description += (description.empty() ? "" : " ") + word.text;
            }
        }
        item.description = Layout::printed(description, evidence, "geometry-row-description");
        if (articleNumbers && descriptionLeft >= 0 && quantityLeft > descriptionLeft && !row.words.empty()) {
            const auto column = Layout::columnText(row, descriptionLeft, quantityLeft);
            if (!column.empty())
                item.description = Layout::printed(column, row, "header-defined-description-column");
        }
        item.printed = Layout::printed(money(last.str()), row, "rightmost-printed-line-amount");

        const auto separator = last.str().find_first_of(",.");
        if (separatedAmount && separator != std::string::npos && last.str().size() - separator - 1 != 2)
            item.printed = {std::nullopt, "unknown", row.text, row.regions, "ambiguous-money-or-code-token"};

        if (suffix.size() == 1 && std::isalnum(static_cast<unsigned char>(suffix[0])))
            item.vatCode = Layout::printed(suffix, row, "trailing-vat-code");

        if (priceColumns && numbers.size() >= 2)
            item.unitPrice =
                Layout::printed(Decimal::read(numbers[numbers.size() - 2].str()), row, "two-price-columns");

        if (pendingMultiplication)
            applyMultiplication(item, *pendingMultiplication);

        if (row.uncertain || evidence.uncertain)
            item.reviewReasons.push_back("source confidence missing or below 85; independent of arithmetic");

        item.reviewReasons.push_back("heuristic row association; verify against source");
        if (!item.quantity.value)
            item.reviewReasons.push_back("quantity not observed; no default one");

        applyReadings(item, row);
        if (correction)
            result.corrections.push_back(std::move(item));
        else
            result.items.push_back(std::move(item));
        pendingDescription.reset();
        pendingMultiplication.reset();
    }
    if (pendingDescription)
        result.reviewReasons.push_back("unresolved description candidate: " + pendingDescription->text);

    std::set<std::string> taxCodes;
    std::set<std::string> printedCodes;
    for (const auto& tax : result.taxes) {
        if (tax.code.value)
            taxCodes.insert(*tax.code.value);
    }
    for (auto& item : result.items) {
        if (!item.marker.value)
            item.marker = item.vatCode;
        item.vatCode = {};
        if (item.marker.value && std::regex_match(*item.marker.value, std::regex(R"([A-Z])")))
            printedCodes.insert(*item.marker.value);
    }
    const bool codeTable = !nutritionLabel && printedCodes.size() >= 2 &&
                           std::ranges::all_of(printedCodes, [&](const auto& code) { return taxCodes.contains(code); });
    for (auto& item : result.items) {
        if (item.marker.value && (explicitVatColumn || (codeTable && taxCodes.contains(*item.marker.value)))) {
            item.vatCode = item.marker;
            item.vatCode.rule += "; meaning supported by printed VAT header/code table";
            for (const auto& tax : result.taxes) {
                if (tax.code.value == item.vatCode.value && tax.rate.value) {
                    item.vatRate = tax.rate;
                    item.vatRate.origin = "derived";
                    item.vatRate.rule = "item code linked to independently printed VAT code table";
                    item.vatRate.regions.insert(item.vatRate.regions.end(), item.vatCode.regions.begin(),
                                                item.vatCode.regions.end());
                }
            }
        } else if (item.marker.value)
            item.reviewReasons.push_back("printed code meaning unknown; no VAT or Nutri-Score inferred");
    }
    reconcile(result);

    return result;
}

} // namespace ReceiptLedger::Engine::Receipt
