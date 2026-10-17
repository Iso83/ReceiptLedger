#include "TestAssert.h"

#include <ReceiptLedger/engine/receipt/Interpreter.h>

using namespace ReceiptLedger::Engine::Receipt;

static ReceiptLedger::Engine::Ocr::Result fixture(const std::vector<std::string>& lines) {
    ReceiptLedger::Engine::Ocr::Result result;
    for (std::size_t index = 0; index < lines.size(); ++index)
        result.regions.push_back({"line-" + std::to_string(index), ReceiptLedger::Engine::Ocr::Level::Line, "",
                                  static_cast<int>(index), lines[index], 0, static_cast<int>(index * 30), 500,
                                  static_cast<int>(index * 30 + 20), 99.0F});

    return result;
}

int test_exact_arithmetic() {
    CPPTEST_ASSERT(Decimal::productCents(Decimal::read("3"), Decimal::read("1,25")) == 375);
    CPPTEST_ASSERT(Decimal::productCents(Decimal::read("0,750"), Decimal::read("4,99")) == 374);
    CPPTEST_ASSERT(Decimal::productCents(Decimal::read("-0,750"), Decimal::read("4,99")) == -374);
    CPPTEST_ASSERT(Decimal::read("0,005").cents() == 1);
    CPPTEST_ASSERT(Decimal::read("-0,005").cents() == -1);
    CPPTEST_ASSERT(Decimal::taxCents(1000, Decimal::read("21")) == 210);
    CPPTEST_ASSERT(Decimal::read("4,990").text() == "4,990");
    CPPTEST_ASSERT(!Decimal::parse("1.234,56"));
    CPPTEST_ASSERT(!Decimal::parse("NaN"));
    CPPTEST_ASSERT(!Decimal::parse("9223372036854775808"));

    bool overflow = false;
    try {
        Decimal::productCents(Decimal::read("9223372036854775807"), Decimal::read("2"));
    } catch (const std::overflow_error&) {
        overflow = true;
    }
    CPPTEST_ASSERT(overflow);

    return 0;
}

int test_interpretation_and_provenance() {
    const auto source =
        fixture({"ALDI", "21/09/2026 09:00 EUR", "Artikel Prijs Totaal", "3 Apples 1,25 3,75", "1 Long description",
                 "continued", "2,00 2,00", "TOTAAL 5,75", "Bancontact 5,75", "Wisselgeld 0,00"});
    const auto result = Interpreter::interpret(source, "doc-a-page-1");
    CPPTEST_ASSERT(result.items.size() == 2);
    CPPTEST_ASSERT(result.items[0].description.value == "Apples");
    CPPTEST_ASSERT(result.items[0].quantity.value->text() == "3");
    CPPTEST_ASSERT(result.items[0].printed.value == 375);
    CPPTEST_ASSERT(result.items[0].amountCheck.status == "match");
    CPPTEST_ASSERT(result.items[1].description.value == "Long description\ncontinued");
    CPPTEST_ASSERT(result.items[1].description.regions.size() == 3);
    CPPTEST_ASSERT(result.totalCheck.status == "match");
    CPPTEST_ASSERT(result.total.value == 575);
    CPPTEST_ASSERT(result.currency.value == "EUR");
    CPPTEST_ASSERT(!result.reviewReasons.empty());

    auto wordsAndLines = source;
    for (const auto& line : source.regions) {
        auto word = line;
        word.id += "-word";
        word.parentId = line.id;
        word.level = ReceiptLedger::Engine::Ocr::Level::Word;
        wordsAndLines.regions.push_back(word);
    }
    CPPTEST_ASSERT(Interpreter::interpret(wordsAndLines, "words").items.size() == 2);

    const auto again = Interpreter::interpret(source, "doc-a-page-1");
    CPPTEST_ASSERT(again.items[0].id == result.items[0].id);
    CPPTEST_ASSERT(Interpreter::interpret(source, "doc-b-page-1").items[0].id != result.items[0].id);
    auto invalid = source;
    invalid.regions[0].parentId = "missing";

    bool rejected = false;
    try {
        Interpreter::interpret(invalid, "invalid");
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    CPPTEST_ASSERT(rejected);

    return 0;
}

int test_unknown_and_compensating_errors() {
    auto result = Interpreter::interpret(
        fixture({"Artikel Prijs Totaal", "3 A 1,25 3,70", "2 B 1,00 2,05", "C 1,25", "TOTAAL 7,00"}), "negative");
    CPPTEST_ASSERT(result.items.size() == 3);
    CPPTEST_ASSERT(result.items[0].amountCheck.status == "mismatch");
    CPPTEST_ASSERT(result.items[0].amountCheck.difference == 5);
    CPPTEST_ASSERT(result.items[1].amountCheck.difference == -5);
    CPPTEST_ASSERT(result.items[2].amountCheck.status == "not_checkable");
    CPPTEST_ASSERT(!result.items[2].quantity.value);
    CPPTEST_ASSERT(result.totalCheck.status == "match");
    CPPTEST_ASSERT(result.calculatedTotalCheck.status == "not_checkable");
    CPPTEST_ASSERT(result.taxTotalCheck.status == "not_checkable");

    auto missing = result;
    missing.items.erase(missing.items.begin());
    Interpreter::reconcile(missing);
    CPPTEST_ASSERT(missing.totalCheck.status == "mismatch");
    result.total.value = 800;
    Interpreter::reconcile(result);
    CPPTEST_ASSERT(result.totalCheck.status == "mismatch");
    CPPTEST_ASSERT(result.totalCheck.difference == -100);

    return 0;
}

int test_weights_taxes_and_corrections() {
    auto result = Interpreter::interpret(fixture({"0,750 kg x 4,99", "Pears 3,74 B", "TOTAAL 3,74"}), "weight");
    CPPTEST_ASSERT(result.items.size() == 1);
    CPPTEST_ASSERT(result.items[0].unit.value == "kg");
    CPPTEST_ASSERT(result.items[0].calculated == 374);
    CPPTEST_ASSERT(result.items[0].amountCheck.status == "match");
    result = Interpreter::interpret(
        fixture({"Artikel Prijs Totaal", "1 A 12,10 12,10", "1 B 1,06 1,06", "Korting -1,00", "Statiegeld 0,50",
                 "-1 Retour 1,00 -1,00", "TOTAAL 11,66", "D 21,00% 10,00 2,10 12,10", "B 6,00% 1,00 0,06 1,06"}),
        "tax");
    CPPTEST_ASSERT(result.items.size() == 3);
    CPPTEST_ASSERT(result.corrections.size() == 2);
    CPPTEST_ASSERT(result.corrections[0].printed.value == -100);
    CPPTEST_ASSERT(result.items[2].printed.value == -100);
    CPPTEST_ASSERT(result.totalCheck.status == "match");
    CPPTEST_ASSERT(result.taxes.size() == 2);
    CPPTEST_ASSERT(result.taxes[0].grossCheck.status == "match");
    CPPTEST_ASSERT(result.taxes[0].rateCheck.status == "match");
    CPPTEST_ASSERT(result.taxTotalCheck.status == "mismatch");

    auto duplicate = result;
    duplicate.taxes.push_back(duplicate.taxes.front());
    Interpreter::reconcile(duplicate);
    CPPTEST_ASSERT(duplicate.taxTotalCheck.status == "mismatch");

    auto zero = Interpreter::interpret(
        fixture({"Artikel Prijs Totaal", "1 A 1,00 1,00", "TOTAAL 1,00", "Z 0,00% 1,00 0,00 1,00"}), "zero");
    CPPTEST_ASSERT(zero.taxes[0].rateCheck.status == "match");
    CPPTEST_ASSERT(zero.taxTotalCheck.status == "match");
    result.taxes[0].tax.value = 420;

    Interpreter::reconcile(result);
    CPPTEST_ASSERT(result.taxes[0].grossCheck.status == "mismatch");
    CPPTEST_ASSERT(result.taxes[0].rateCheck.status == "mismatch");
    CPPTEST_ASSERT(Check::compare(std::nullopt, 0).status == "not_checkable");
    CPPTEST_ASSERT(Check::compare(100, 101).status == "mismatch");

    auto unitTable = Interpreter::interpret(
        fixture({"Omschrijving Prijs", "ADC020614 Kalksteen antraciet 6/14 Ton 0,06 94,80 5,69 21",
                 "XAZW000463 Afname tot 300kg kg 60,000 0,050 3 21", "TOTAAL EUR 8,69"}),
        "units");
    CPPTEST_ASSERT(unitTable.items.size() == 2);
    CPPTEST_ASSERT(unitTable.items[0].calculated == 569);
    CPPTEST_ASSERT(unitTable.items[1].calculated == 300);
    CPPTEST_ASSERT(unitTable.totalCheck.status == "match");

    auto linkedDiscount = Interpreter::interpret(
        fixture({"Artikel Prijs Totaal", "3 A 1,25 3,25", "Artikelkorting -0,50", "TOTAAL 3,25"}), "discount");
    CPPTEST_ASSERT(linkedDiscount.items[0].discount.value == -50);
    CPPTEST_ASSERT(linkedDiscount.items[0].amountCheck.status == "match");

    return 0;
}

int test_column_readings_and_code_meaning() {
    auto source = fixture({"12x0657e", "Cola 8,048", "Water 1,00 B", "Soap 2,42 D", "TOTAAL 11,46",
                           "B 6,00% 8,53 0,51 9,04", "D 21,00% 2,00 0,42 2,42"});
    source.fieldReadings.push_back({"crop-quantity", ReceiptLedger::Engine::Ocr::Level::Line, "line-0", 0, "12 x 0,67",
                                    100, 1, 300, 20, 95.0F, "multiplication"});
    source.fieldReadings.push_back({"crop-amount", ReceiptLedger::Engine::Ocr::Level::Line, "line-1", 0, "8,04", 400,
                                    31, 470, 50, 95.0F, "amount"});
    source.fieldReadings.push_back(
        {"crop-code", ReceiptLedger::Engine::Ocr::Level::Line, "line-1", 0, "B", 480, 31, 490, 50, 95.0F, "marker"});

    auto result = Interpreter::interpret(source, "columns");
    CPPTEST_ASSERT(result.items.size() == 3);
    CPPTEST_ASSERT(result.items[0].printed.value == 804);
    CPPTEST_ASSERT(result.items[0].quantity.value->text() == "12");
    CPPTEST_ASSERT(result.items[0].unitPrice.value->text() == "0,67");
    CPPTEST_ASSERT(result.items[0].printed.regions.front() == "crop-amount");
    CPPTEST_ASSERT(!result.items[0].printed.alternatives.empty());
    CPPTEST_ASSERT(source.regions[1].text == "Cola 8,048");
    CPPTEST_ASSERT(result.items[0].vatCode.value == "B");
    CPPTEST_ASSERT(result.items[0].vatRate.value->text() == "6,00");
    CPPTEST_ASSERT(result.items[0].vatRate.origin == "derived");
    CPPTEST_ASSERT(result.totalCheck.status == "match");
    CPPTEST_ASSERT(result.taxes[0].itemsGrossCheck.status == "match");
    CPPTEST_ASSERT(result.taxes[1].itemsGrossCheck.status == "match");

    auto wrongItem = result;
    wrongItem.items[0].printed.value = 800;
    Interpreter::reconcile(wrongItem);
    CPPTEST_ASSERT(wrongItem.taxes[0].itemsGrossCheck.difference == -4);
    CPPTEST_ASSERT(wrongItem.taxes[0].grossCheck.status == "match");
    source.regions[1].text = "Cola 8,04 B";
    source.fieldReadings[1].text = "8,09";

    result = Interpreter::interpret(source, "conflicting-reread");
    CPPTEST_ASSERT(result.items[0].printed.value == 804);
    CPPTEST_ASSERT(result.items[0].printed.alternatives.front() == "8,09");
    source.regions[2].text = "Water 1,00 A";
    result = Interpreter::interpret(source, "unknown-marketing-code");
    CPPTEST_ASSERT(result.items[1].marker.value == "A");
    CPPTEST_ASSERT(!result.items[0].vatCode.value);
    source.regions[2].text = "NUTRI-SCORE Water 1,00 B";
    result = Interpreter::interpret(source, "nutrition");
    CPPTEST_ASSERT(result.items[0].marker.value == "B");
    CPPTEST_ASSERT(!result.items[0].vatCode.value);
    auto uncertainDescription = fixture({"garbled header", "2,00 2,00 1", "TOTAAL 2,00"});
    uncertainDescription.fieldReadings.push_back({"crop-description", ReceiptLedger::Engine::Ocr::Level::Line, "line-0",
                                                  0, "HEADER\n1 PRODUCT A\n", 0, 0, 200, 40, 35.0F, "description"});

    const auto candidate = Interpreter::interpret(uncertainDescription, "uncertain-description");
    CPPTEST_ASSERT(candidate.items.size() == 1);
    CPPTEST_ASSERT(candidate.items[0].description.value == "PRODUCT A");
    CPPTEST_ASSERT(candidate.items[0].description.regions.front() == "crop-description");
    CPPTEST_ASSERT(!candidate.items[0].reviewReasons.empty());
    CPPTEST_ASSERT(uncertainDescription.regions[0].text == "garbled header");
    source.fieldReadings[0].parentId = "missing";

    bool rejected = false;
    try {
        Interpreter::interpret(source, "invalid-crop");
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    CPPTEST_ASSERT(rejected);

    return 0;
}

int test_description_column_excludes_article_number_and_quantity() {
    using ReceiptLedger::Engine::Ocr::Level;

    auto source = fixture({"ArtNr Benaming Hoev. Prijs Bedrag", "9183GOUDA 2 1,25 2,50", "TOTAAL 2,50"});
    const auto word = [&](std::string id, std::string parent, std::string text, int left, int right, int top) {
        source.regions.push_back({id, Level::Word, parent, 0, text, left, top, right, top + 20, 99.0F});
    };
    word("header-id", "line-0", "ArtNr", 10, 60, 0);
    word("header-name", "line-0", "Benaming", 100, 200, 0);
    word("header-qty", "line-0", "Hoev.", 300, 350, 0);
    word("header-price", "line-0", "Prijs", 400, 450, 0);
    word("header-amount", "line-0", "Bedrag", 500, 580, 0);
    word("mixed", "line-1", "9183GOUDA", 10, 200, 30);
    word("qty", "line-1", "2", 310, 320, 30);
    word("price", "line-1", "1,25", 400, 440, 30);
    word("amount", "line-1", "2,50", 500, 540, 30);
    const std::string text = "9183GOUDA";
    for (std::size_t index = 0; index < text.size(); ++index) {
        const int left = index < 4 ? 10 + static_cast<int>(index) * 15 : 100 + static_cast<int>(index - 4) * 15;
        source.regions.push_back({"glyph-" + std::to_string(index), Level::Symbol, "mixed", 0, text.substr(index, 1),
                                  left, 30, left + 10, 50, 99.0F});
    }

    const auto result = Interpreter::interpret(source, "column-boundaries");
    CPPTEST_ASSERT(result.items.size() == 1);
    CPPTEST_ASSERT(result.items[0].description.value == "GOUDA");
    CPPTEST_ASSERT(result.items[0].description.rule == "header-defined-description-column");
    CPPTEST_ASSERT(result.items[0].quantity.value->text() == "2");
    CPPTEST_ASSERT(result.items[0].printed.value == 250);
    CPPTEST_ASSERT(source.regions[1].text == "9183GOUDA 2 1,25 2,50");
    return 0;
}

int main() {
    CPPTEST_RUN(test_description_column_excludes_article_number_and_quantity);
    CPPTEST_RUN(test_column_readings_and_code_meaning);
    CPPTEST_RUN(test_exact_arithmetic);
    CPPTEST_RUN(test_interpretation_and_provenance);
    CPPTEST_RUN(test_unknown_and_compensating_errors);
    CPPTEST_RUN(test_weights_taxes_and_corrections);
    return 0;
}
