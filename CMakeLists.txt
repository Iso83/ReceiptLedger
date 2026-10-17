cmake_minimum_required(VERSION 4.0)

project(ReceiptLedger
    VERSION 0.1.0
    LANGUAGES C CXX
)

# =========================================================
# Project configuration
# =========================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# =========================================================
# CppCMake
# =========================================================

include(cmake/Dependencies.cmake)

dependencies_fetch_cppcmake()

cppcmake_project_initialize()

if(CPPCMAKE_PROJECT_ALREADY_REGISTERED)
    return()
endif()

cppcmake_githook_precommit()
cppcmake_version_generate()
cppcmake_test_setup()

# =========================================================
# Dependencies
# =========================================================

cppcmake_dependency_try_local_packages()

cppcmake_gitsubmodule_init_missing_targets(
    WORKING_DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}"

    PATH
        "extern/CppDependencies"
)

include(extern/CppDependencies/CppDependencies.cmake)

include(GNUInstallDirs)

cppdependencies_zlib(ZLIB SHARED)
cppdependencies_png(PNG SHARED)
cppdependencies_jpeg(JPEG SHARED)
cppdependencies_tiff(TIFF SHARED)
cppdependencies_leptonica(LEPTONICA SHARED)
cppdependencies_tesseract(TESSERACT SHARED
    TESSDATA_DIR RECEIPTLEDGER_TESSDATA_DIR
)
cppdependencies_cli11(CLI11)
cppdependencies_json_nlohmann(JSON)
cppdependencies_pdfium(PDFIUM SHARED)

cppdependencies_opencv(OPENCV SHARED
    COMPONENTS core imgproc imgcodecs
    ZLIB_TARGET ${ZLIB}
    PNG_TARGET ${PNG}
    JPEG_TARGET ${JPEG}
    TIFF_TARGET ${TIFF}
)

# =========================================================
# Modules
# =========================================================

cppcmake_module_add(engine/ocr)
cppcmake_module_add(engine/receipt)
cppcmake_module_add(apps/pdf2csv)
