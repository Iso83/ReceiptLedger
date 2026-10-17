#include "Application.h"

#include <CLI/CLI.hpp>
#include <cstdlib>
#include <exception>
#include <iostream>

// .\pdf2cvs --input tickets\*.pdf --output processed --debug

int main(int argc, char** argv) {

    ReceiptLedger::App::PDF2CSV::Options options;
    options.tessdata = RECEIPTLEDGER_DEFAULT_TESSDATA_DIR;

    CLI::App cli("Interpret receipt PDFs, export items/receipts and retain raw Tesseract regions");
    cli.add_option("--input", options.inputs,
                   "PDF files, folders (non-recursive), or filename patterns such as tickets/*.pdf")
        ->required();
    cli.add_option("--output", options.output, "Output directory")->required();
    cli.add_option("--tessdata", options.tessdata, "Tesseract model directory");
    cli.add_option("--language", options.language, "Tesseract language");
    cli.add_option("--dpi", options.dpi, "Render/source DPI");
    cli.add_option("--psm", options.psm, "Tesseract page segmentation mode");
    cli.add_option("--confidence-high", options.confidenceHigh, "High-score boundary");
    cli.add_option("--confidence-low", options.confidenceLow, "Low-score boundary");
    cli.add_flag("--debug", options.debug, "Write render, OCR input and overlay PNGs");

    CLI11_PARSE(cli, argc, argv);
    try {
        return ReceiptLedger::App::PDF2CSV::Application::run(options);
    } catch (const std::exception& error) {
        std::cerr << "PDF2CSV error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
