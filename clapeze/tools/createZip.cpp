#include <miniz.h>
#include <miniz_zip.h>

int main(int argc, const char* argv[]) {
    if (argc < 3) {
        printf("usage: %s <archive.zip> [file1, file2...]\n", argv[0]);
        return 0;
    }
    const char* outputFile = argv[1];
    mz_zip_archive zip;
    mz_zip_archive* pZip = &zip;
    mz_zip_zero_struct(pZip);
    mz_uint64 initialSize = 0;
    printf("Creating new archive: %s\n", outputFile);
    if (!mz_zip_writer_init_file(pZip, outputFile, initialSize)) {
        mz_zip_error e = mz_zip_get_last_error(pZip);
        printf("ERROR: %s\n", mz_zip_get_error_string(e));
        return 1;
    }

    for (uint32_t idx = 2; idx < argc; ++idx) {
        const char* targetPath = argv[idx];
        const char* srcPath = argv[idx];
        printf("Adding %s\n", srcPath);
        if (!mz_zip_writer_add_file(pZip, targetPath, srcPath, nullptr, 0, MZ_NO_COMPRESSION)) {
            mz_zip_error e = mz_zip_get_last_error(pZip);
            printf("ERROR: %s\n", mz_zip_get_error_string(e));
            return 1;
        }
    }

    printf("Finalizing archive...\n");
    if (!mz_zip_writer_finalize_archive(pZip)) {
        mz_zip_error e = mz_zip_get_last_error(pZip);
        printf("ERROR: %s\n", mz_zip_get_error_string(e));
        return 1;
    }
    mz_zip_writer_end(pZip);
    printf("Archive complete!\n");
    return 0;
}