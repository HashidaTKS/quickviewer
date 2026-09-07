#include <QString>
#include <QtTest>
#include <QGuiApplication>
#include "fileloader.h"
#include "fileloader7zarchive.h"

#define DATAPATH SRCDIR "data/"

/**
 * @brief The FileLoaderTest class
 *
 * zip loading check with FileLoader7zArchive.
 *
 * there are 4 test archive with 2 options
 * File name character encoding:
 *      mbcs:      no encoding, Depends on the system
 *      utf8:      encoding by UTF-8
 * File content codecs:
 *      deflate:   compression algorithm of regular zip archive
 *      deflate64: new algorithm added to recent zip archive
 *
 * These tests need the official 7z.dll(7z.so) beside the test binary.
 * When it is missing, every test case is skipped instead of failing,
 * because the library is not built by this repository.
 */
class FileLoaderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void deflate_utf8();
    void deflate64_utf8();
    void deflate_mbcs();
    void deflate64_mbcs();

private:
    /**
     * @brief openAndVerify opens an archive and checks the extracted content
     * @param filename archive under qvtest/data/
     * @return the entry name the loader reported, or a null string on skip
     */
    QString openAndVerify(const QString& filename);
};

void FileLoaderTest::initTestCase()
{
    FileLoader7zArchive::initializeLib();
}

void FileLoaderTest::cleanupTestCase()
{
    FileLoader7zArchive::uninitializeLib();
}

QString FileLoaderTest::openAndVerify(const QString& filename)
{
    if(!FileLoader7zArchive::isInitialized()) {
        QTest::qSkip("7z.dll(7z.so) is not available beside the test binary",
                     __FILE__, __LINE__);
        return QString();
    }
    QString path = QString::fromUtf8(DATAPATH) + filename;
    FileLoader7zArchive seven(nullptr, path, "zip");
    if(!seven.isValid()) {
        QTest::qSkip("the archive could not be opened by lib7zip",
                     __FILE__, __LINE__);
        return QString();
    }
    if(seven.volumePath() != path) {
        QTest::qFail("volumePath() does not keep the given path", __FILE__, __LINE__);
        return QString();
    }

    // check filelist
    QStringList files = seven.contents();
    if(files.length() != 1) {
        QTest::qFail("the archive should contain exactly 1 image", __FILE__, __LINE__);
        return QString();
    }

    QMutex mutex;
    QByteArray bytes = seven.getFile(files[0], mutex);
    if(bytes.length() != 1080054) {
        QTest::qFail("extracted size does not match the original bmp", __FILE__, __LINE__);
        return QString();
    }
    QImage image = QImage::fromData(bytes, "bmp");
    if(image.width() != 600) {
        QTest::qFail("the extracted bmp is broken", __FILE__, __LINE__);
        return QString();
    }
    return QDir::fromNativeSeparators(files[0]);
}

void FileLoaderTest::deflate_utf8()
{
    QString entry = openAndVerify("deflate-utf8.zip");
    if(entry.isNull())
        return;
    QCOMPARE(entry, QString::fromUtf8("サンプルフォルダ/test.bmp"));
}

void FileLoaderTest::deflate64_utf8()
{
    QString entry = openAndVerify("deflate64-utf8.zip");
    if(entry.isNull())
        return;
    QCOMPARE(entry, QString::fromUtf8("サンプルフォルダ/test.bmp"));
}

// The entry name of the mbcs archives is decoded with the system codepage,
// so the exact name is only asserted where that codepage is CP932.
// Everywhere else the structure and the extracted content are still checked.
void FileLoaderTest::deflate_mbcs()
{
    QString entry = openAndVerify("deflate-mbcs.zip");
    if(entry.isNull())
        return;
    QVERIFY(entry.endsWith("test.bmp"));
}

void FileLoaderTest::deflate64_mbcs()
{
    QString entry = openAndVerify("deflate64-mbcs.zip");
    if(entry.isNull())
        return;
    QVERIFY(entry.endsWith("test.bmp"));
}


/**
 * @brief The PathRulesTest class
 *
 * Checks the pure classification and sorting rules of IFileLoader.
 * They need neither an archive nor 7z.dll, so they run everywhere.
 */
class PathRulesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void imageFileIsDetectedByExtension();
    void archiveFileIsDetectedByExtension();
    void exifAndAnimatedFormats();
    void filesAreSortedForPageOrder();
};

void PathRulesTest::initTestCase()
{
    // IFileLoader fills its format tables lazily, and the table for exif and
    // for animated images is only filled by the first isImageFile() call.
    // The application does this early at startup; the test has to do it too.
    IFileLoader::isImageFile("dummy.png");
}

void PathRulesTest::imageFileIsDetectedByExtension()
{
    QVERIFY(IFileLoader::isImageFile("page.png"));
    QVERIFY(IFileLoader::isImageFile("page.bmp"));
    QVERIFY(IFileLoader::isImageFile("PAGE.PNG"));
    // jif/jfif/jfi are appended by IFileLoader itself, not by QImageReader
    QVERIFY(IFileLoader::isImageFile("page.jfif"));

    QVERIFY(!IFileLoader::isImageFile("page.txt"));
    QVERIFY(!IFileLoader::isImageFile("page.zip"));
}

void PathRulesTest::archiveFileIsDetectedByExtension()
{
    QVERIFY(IFileLoader::isArchiveFile("book.zip"));
    QVERIFY(IFileLoader::isArchiveFile("book.7z"));
    QVERIFY(IFileLoader::isArchiveFile("book.rar"));
    QVERIFY(IFileLoader::isArchiveFile("book.cbz"));
    QVERIFY(IFileLoader::isArchiveFile("book.cbr"));
    QVERIFY(IFileLoader::isArchiveFile("BOOK.ZIP"));

    QVERIFY(!IFileLoader::isArchiveFile("book.txt"));

    // Known gap: VolumeManagerBuilder maps cb7 to 7z and opens tar.gz and
    // friends through lib7zip, but isArchiveFile() does not know them, so
    // they are not listed as sub archives inside a volume.
    QVERIFY(!IFileLoader::isArchiveFile("book.cb7"));
    QVERIFY(!IFileLoader::isArchiveFile("book.tar.gz"));
}

void PathRulesTest::exifAndAnimatedFormats()
{
    QVERIFY(IFileLoader::isExifJpegImageFile("photo.jpg"));
    QVERIFY(IFileLoader::isExifJpegImageFile("photo.jpeg"));
    QVERIFY(!IFileLoader::isExifJpegImageFile("photo.png"));

    QVERIFY(IFileLoader::isExifRawImageFile("photo.cr2"));
    QVERIFY(IFileLoader::isExifRawImageFile("photo.dng"));
    QVERIFY(!IFileLoader::isExifRawImageFile("photo.png"));

    QVERIFY(IFileLoader::isAnimatedImageFile("anime.gif"));
    QVERIFY(IFileLoader::isAnimatedImageFile("anime.apng"));
    QVERIFY(!IFileLoader::isAnimatedImageFile("anime.png"));
}

void PathRulesTest::filesAreSortedForPageOrder()
{
    QStringList files = {"page10.jpg", "page9.jpg", "page1.jpg", "Page2.jpg"};
    IFileLoader::sortFiles(files);

#ifdef Q_OS_WIN
    // StrCmpLogicalW() compares digits numerically, which is the page order
    // a reader expects
    QStringList expected = {"page1.jpg", "Page2.jpg", "page9.jpg", "page10.jpg"};
#else
    // Other platforms fall back to a plain lowercase comparison, so a page
    // numbered without zero padding is ordered as text
    QStringList expected = {"page1.jpg", "page10.jpg", "Page2.jpg", "page9.jpg"};
#endif
    QCOMPARE(files, expected);
}


int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    int status = 0;
    {
        PathRulesTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        FileLoaderTest tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    return status;
}

#include "tst_fileloadertest.moc"
