// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filetypeicons.h"

#include <optional>

#include <QFileInfo>
#include <QMimeDatabase>
#include <QSet>

#include "desktoputils.h"

using namespace Qt::StringLiterals;

namespace tremotesf::filetypeicons {
    namespace {
        std::optional<QLatin1String> fileTypeIconName(const QString& suffix) {
            static const QSet<QString> videoSuffixes{
                u"mp4"_s,
                u"mkv"_s,
                u"avi"_s,
                u"mov"_s,
                u"wmv"_s,
                u"flv"_s,
                u"webm"_s,
                u"m4v"_s,
                u"mpg"_s,
                u"mpeg"_s,
                u"ts"_s,
                u"m2ts"_s,
                u"vob"_s,
                u"3gp"_s,
                u"ogv"_s,
                u"divx"_s,
                u"rmvb"_s,
                u"asf"_s
            };
            static const QSet<QString> audioSuffixes{
                u"mp3"_s,
                u"flac"_s,
                u"wav"_s,
                u"aac"_s,
                u"ogg"_s,
                u"m4a"_s,
                u"opus"_s,
                u"wma"_s,
                u"ape"_s,
                u"aiff"_s,
                u"alac"_s,
                u"mid"_s,
                u"midi"_s,
                u"mka"_s
            };
            static const QSet<QString> imageSuffixes{
                u"jpg"_s,
                u"jpeg"_s,
                u"png"_s,
                u"gif"_s,
                u"bmp"_s,
                u"webp"_s,
                u"tiff"_s,
                u"tif"_s,
                u"svg"_s,
                u"ico"_s,
                u"heic"_s,
                u"heif"_s,
                u"avif"_s,
                u"raw"_s,
                u"psd"_s
            };
            static const QSet<QString> archiveSuffixes{
                u"zip"_s,
                u"rar"_s,
                u"7z"_s,
                u"tar"_s,
                u"gz"_s,
                u"bz2"_s,
                u"xz"_s,
                u"zst"_s,
                u"iso"_s,
                u"cab"_s,
                u"lz"_s,
                u"lzma"_s,
                u"arj"_s,
                u"tgz"_s,
                u"tbz2"_s,
                u"txz"_s
            };
            static const QSet<QString>
                textSuffixes{u"txt"_s, u"log"_s, u"nfo"_s, u"srt"_s, u"sub"_s, u"ass"_s, u"ssa"_s, u"cue"_s, u"md"_s};
            static const QSet<QString>
                documentSuffixes{u"doc"_s, u"docx"_s, u"odt"_s, u"rtf"_s, u"pages"_s, u"wps"_s, u"tex"_s};
            static const QSet<QString>
                spreadsheetSuffixes{u"xls"_s, u"xlsx"_s, u"ods"_s, u"csv"_s, u"tsv"_s, u"numbers"_s};
            static const QSet<QString> presentationSuffixes{u"ppt"_s, u"pptx"_s, u"odp"_s, u"key"_s};
            static const QSet<QString> codeSuffixes{
                u"c"_s,    u"h"_s,   u"cpp"_s, u"cxx"_s,  u"hpp"_s,  u"cc"_s,   u"hh"_s,   u"cs"_s,    u"java"_s,
                u"py"_s,   u"js"_s,  u"jsx"_s, u"ts"_s,   u"tsx"_s,  u"json"_s, u"yaml"_s, u"yml"_s,   u"xml"_s,
                u"html"_s, u"htm"_s, u"css"_s, u"scss"_s, u"less"_s, u"sh"_s,   u"bash"_s, u"bat"_s,   u"cmd"_s,
                u"ps1"_s,  u"php"_s, u"rb"_s,  u"go"_s,   u"rs"_s,   u"pl"_s,   u"lua"_s,  u"swift"_s, u"kt"_s,
                u"kts"_s,  u"sql"_s, u"ini"_s, u"cfg"_s,  u"toml"_s, u"diff"_s, u"patch"_s
            };

            if (videoSuffixes.contains(suffix)) {
                return "tremotesf-file-video"_L1;
            }
            if (audioSuffixes.contains(suffix)) {
                return "tremotesf-file-audio"_L1;
            }
            if (imageSuffixes.contains(suffix)) {
                return "tremotesf-file-image"_L1;
            }
            if (archiveSuffixes.contains(suffix)) {
                return "tremotesf-file-archive"_L1;
            }
            if (textSuffixes.contains(suffix)) {
                return "tremotesf-file-text"_L1;
            }
            if (documentSuffixes.contains(suffix)) {
                return "tremotesf-file-document"_L1;
            }
            if (spreadsheetSuffixes.contains(suffix)) {
                return "tremotesf-file-spreadsheet"_L1;
            }
            if (presentationSuffixes.contains(suffix)) {
                return "tremotesf-file-presentation"_L1;
            }
            if (codeSuffixes.contains(suffix)) {
                return "tremotesf-file-code"_L1;
            }
            if (suffix == u"pdf"_s) {
                return "tremotesf-file-pdf"_L1;
            }
            return std::nullopt;
        }
    }

    QIcon fileIcon(const QString& fileName) {
        const auto suffix = QFileInfo(fileName).suffix().toLower();
        if (!suffix.isEmpty()) {
            if (const auto iconName = fileTypeIconName(suffix); iconName.has_value()) {
                if (const auto icon = QIcon::fromTheme(*iconName); !icon.isNull()) {
                    return icon;
                }
            }
        }
        static const QMimeDatabase mimeDb{};
        const auto mimeType = mimeDb.mimeTypeForFile(fileName, QMimeDatabase::MatchExtension);
        if (mimeType.isValid()) {
            if (const auto icon = QIcon::fromTheme(mimeType.iconName()); !icon.isNull()) {
                return icon;
            }
            if (const auto icon = QIcon::fromTheme(mimeType.genericIconName()); !icon.isNull()) {
                return icon;
            }
        }
        if (const auto icon = QIcon::fromTheme("tremotesf-file-generic"_L1); !icon.isNull()) {
            return icon;
        }
        return desktoputils::standardFileIcon();
    }

    const QIcon& folderIcon() {
        static const QIcon icon = QIcon::fromTheme("tremotesf-folder"_L1, desktoputils::standardDirIcon());
        return icon;
    }
}
