#include "TestAssert.h"

#include <ReceiptLedger/engine/ocr/Engine.h>
#include <fstream>
#include <opencv2/imgproc.hpp>

using namespace ReceiptLedger::Engine::Ocr;

int test_real_recognition_and_fresh_state() {
    cv::Mat image(180, 700, CV_8UC1, cv::Scalar(255));
    cv::putText(image, "TEST 123", {20, 120}, cv::FONT_HERSHEY_SIMPLEX, 2.5, cv::Scalar(0), 5, cv::LINE_AA);
    Engine engine(RECEIPTLEDGER_DEFAULT_TESSDATA_DIR, "nld", 300, 7);

    const Result result = engine.recognize(image, "fixture-page-0001");
    CPPTEST_ASSERT(result.text.find("TEST") != std::string::npos);
    CPPTEST_ASSERT(result.text.find("123") != std::string::npos);

    const auto word = std::find_if(result.regions.begin(), result.regions.end(), [](const Region& region) {
        return region.level == Level::Word && region.text.find("TEST") != std::string::npos;
    });
    CPPTEST_ASSERT(word != result.regions.end());
    CPPTEST_ASSERT(word->left >= 0 && word->top >= 0 && word->right <= image.cols && word->bottom <= image.rows);

    const auto symbol = std::find_if(result.regions.begin(), result.regions.end(), [&](const Region& region) {
        return region.level == Level::Symbol && region.parentId == word->id;
    });
    CPPTEST_ASSERT(symbol != result.regions.end());
    CPPTEST_ASSERT(symbol->right > symbol->left);

    CPPTEST_ASSERT(engine.metadata().engineVersion.find("5.") == 0);
    CPPTEST_ASSERT(engine.metadata().language == "nld");
    CPPTEST_ASSERT(engine.metadata().modelSha256 == "92e7a1ad4bf8082e268de57c7823316ec024935702c6ed2a1e473b3a071aa733");

    for (const auto& region : result.regions) {
        CPPTEST_ASSERT(!region.text.empty());
        CPPTEST_ASSERT(region.left >= 0 && region.top >= 0);
        CPPTEST_ASSERT(region.right > region.left && region.right <= image.cols);
        CPPTEST_ASSERT(region.bottom > region.top && region.bottom <= image.rows);
        if (region.level == Level::Word)
            CPPTEST_ASSERT(std::ranges::any_of(result.regions, [&](const Region& parent) {
                return parent.id == region.parentId && parent.level == Level::Line;
            }));
        if (region.level == Level::Symbol)
            CPPTEST_ASSERT(std::ranges::any_of(result.regions, [&](const Region& parent) {
                return parent.id == region.parentId && parent.level == Level::Word;
            }));
    }

    cv::Mat white(180, 700, CV_8UC1, cv::Scalar(255));
    const Result empty = engine.recognize(white, "fixture-page-0002");
    CPPTEST_ASSERT(empty.text.empty());
    CPPTEST_ASSERT(empty.regions.empty());

    cv::Mat storage(200, 740, CV_8UC1, cv::Scalar(255));
    cv::Mat roi = storage(cv::Rect(20, 10, 700, 180));
    cv::putText(roi, "FRESH 456", {20, 120}, cv::FONT_HERSHEY_SIMPLEX, 2.5, cv::Scalar(0), 5, cv::LINE_AA);
    const Result fresh = engine.recognize(roi, "fixture-page-0003");
    CPPTEST_ASSERT(fresh.text.find("FRESH") != std::string::npos);
    CPPTEST_ASSERT(fresh.text.find("TEST") == std::string::npos);

    return 0;
}

int test_rejects_unsupported_layouts() {
    Engine engine(RECEIPTLEDGER_DEFAULT_TESSDATA_DIR, "nld", 300, 7);
    for (const auto type : {CV_8UC3, CV_16UC1}) {
        try {
            engine.recognize(cv::Mat(20, 20, type), "invalid-layout");
        } catch (const std::invalid_argument&) {
            continue;
        }
        CPPTEST_ASSERT(false);
    }

    return 0;
}

int test_model_identity_and_corruption() {
    const auto directory = std::filesystem::temp_directory_path() / "receiptledger-model-identity";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto source = std::filesystem::path(RECEIPTLEDGER_DEFAULT_TESSDATA_DIR) / "nld.traineddata";
    const auto copied = directory / "copy.traineddata";
    std::filesystem::copy_file(source, copied);
    Engine copiedEngine(directory, "copy", 300, 7);
    CPPTEST_ASSERT(copiedEngine.metadata().modelPath == copied);
    CPPTEST_ASSERT(copiedEngine.metadata().modelSha256 ==
                   "92e7a1ad4bf8082e268de57c7823316ec024935702c6ed2a1e473b3a071aa733");

    std::ofstream(directory / "broken.traineddata", std::ios::binary) << "not trained data";
    bool rejected = false;
    try {
        Engine broken(directory, "broken", 300, 7);
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).find("could not load") != std::string::npos;
    }
    CPPTEST_ASSERT(rejected);

    return 0;
}

int test_field_crop_and_rotation_preserve_source() {
    Engine engine(RECEIPTLEDGER_DEFAULT_TESSDATA_DIR, "nld", 300, 3);

    cv::Mat upright(450, 850, CV_8UC1, cv::Scalar(255));
    cv::putText(upright, "SHOP TEST", {40, 100}, cv::FONT_HERSHEY_SIMPLEX, 2.0, cv::Scalar(0), 4, cv::LINE_AA);
    cv::putText(upright, "APPLES 123", {40, 230}, cv::FONT_HERSHEY_SIMPLEX, 2.0, cv::Scalar(0), 4, cv::LINE_AA);
    cv::putText(upright, "TOTAL 456", {40, 360}, cv::FONT_HERSHEY_SIMPLEX, 2.0, cv::Scalar(0), 4, cv::LINE_AA);

    auto field = engine.readField(upright, 25, 35, 500, 115, "parent", "test");
    CPPTEST_ASSERT(field.parentId == "parent");
    CPPTEST_ASSERT(field.id == "parent-field-test");
    CPPTEST_ASSERT(field.text.find("SHOP") != std::string::npos);
    CPPTEST_ASSERT(field.left == 25 && field.right == 500);
    CPPTEST_ASSERT(engine.recognize(upright, "fresh").text.find("456") != std::string::npos);
    cv::Mat inverted;
    cv::rotate(upright, inverted, cv::ROTATE_180);

    const auto recognition = engine.recognizeReceipt(inverted, "rotated");
    CPPTEST_ASSERT(recognition.rotation == 180);
    CPPTEST_ASSERT(recognition.interpreted.text.find("APPLES") != std::string::npos);
    CPPTEST_ASSERT(recognition.interpreted.text.find("456") != std::string::npos);
    CPPTEST_ASSERT(!recognition.rotationReason.empty());
    CPPTEST_ASSERT(recognition.source.regions.front().id.starts_with("rotated-line-"));
    CPPTEST_ASSERT(recognition.interpreted.regions.front().id.starts_with("rotated-rotation-180-line-"));

    bool rejected = false;
    try {
        engine.readField(upright, -1, 0, 30, 30, "invalid", "amount");
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    CPPTEST_ASSERT(rejected);

    return 0;
}

int main() {
    CPPTEST_RUN(test_field_crop_and_rotation_preserve_source);
    CPPTEST_RUN(test_real_recognition_and_fresh_state);
    CPPTEST_RUN(test_rejects_unsupported_layouts);
    CPPTEST_RUN(test_model_identity_and_corruption);
    return 0;
}
