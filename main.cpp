#include <string>
#include <cstdlib>
#include <vector>
#include <map>

#ifdef _MSC_VER
// Required define for MSVC compatibility
#define __STDC__ 1
#endif

#include "getopt/getopt.h"

#include "file_io.h"
#include "common_types.h"
#include "ncch.h"
#include "ncsd.h"

typedef std::map<std::string, const char*> optlist;

bool DecryptNCCH(const optlist& args, NCCH* ncch)
{
    if (args.find("exefs") == args.end() && args.find("exefs7") != args.end()) {
        printf("ERROR: 7.x EXEFS XORPads must be accompanied by normal EXEFS XORPads!\n");
        return false;
    }

    switch(ncch->GetType()) {
        case NCCH::TYPE_CFA:
            printf("ERROR: CFA not yet supported!\n");
            return false;
        case NCCH::TYPE_CXI: {
            if (args.find("exheader") == args.end()) {
                printf("ERROR: The input file type requires an exheader xorpad!\n");
                return false;
            }

            if (!ncch->DecryptExheader(ReadBinaryFile(args.at("exheader"))))
                return false;

            if (args.find("exefs") == args.end()) {
                printf("ERROR: CXIs without EXEFSs not yet supported!\n");
                return false;
            }

            if (args.find("exefs7") == args.end()) {
                if (!ncch->DecryptEXEFS(ReadBinaryFile(args.at("exefs")))) 
                    return false;
            } else if (!ncch->DecryptEXEFS(ReadBinaryFile(args.at("exefs")), ReadBinaryFile(args.at("exefs7")))) {
                    return false;
            }
            
            if (ncch->HasRomFS()) {
                if (args.find("romfs") == args.end()) {
                    printf("ERROR: The input file type requires a ROMFS xorpad!\n");
                    return false;
                }
                if (!ncch->DecryptROMFS(ReadBinaryFile(args.at("romfs")))) 
                    return false;
            }

            ncch->SetDecrypted();
            return true;
        }
    }
    
    return false;
}

bool DecryptNCSD(const optlist& args, NCSD* ncsd)
{
    // We'll only decrypt the first NCCH
    NCCH ncch = ncsd->GetNCCH(0);
    printf("WARNING: Only decryption of the first NCCH is supported!\n");
    return DecryptNCCH(args, &ncch);
}

void ShowHelpInfo()
{
    printf("xorer: Apply XORPads to encrypted 3DS files\n");
    printf("Usage: xorer <file> [-e xorpad] [-x xorpad] [-r xorpad] [-7 xorpad]\n");
    printf("  -h  --help      Display this help information\n");
    printf("  -e  --exheader  Specify the Exheader XORPad\n");
    printf("  -x  --exefs     Specify the (normal) EXEFS Xorpad\n");
    printf("  -r  --romfs     Specify the ROMFS Xorpad\n");
    printf("  -7  --exefs7    Specify the 7.x EXEFS Xorpad\n");
    exit(1);
}

int main(int argc, char** argv)
{
    optlist prsd;
    int c;
    while (true)
    {
        int option_index;
        static struct option long_options[] =
        {
            { "help",     no_argument,       nullptr, 'h'},
            { "exheader", required_argument, nullptr, 'e'},
            { "exefs",    required_argument, nullptr, 'x'},
            { "romfs",    required_argument, nullptr, 'r'},
            { "exefs7",   required_argument, nullptr, '7'},
            { 0 },
        };

        c = getopt_long(argc, argv, "he:x:r:7:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c)
        {
            case 'e': prsd["exheader"] = optarg; break;
            case 'x': prsd["exefs"]    = optarg; break;
            case 'r': prsd["romfs"]    = optarg; break;
            case '7': prsd["exefs7"]   = optarg; break;

            case 'h':
            default:
                ShowHelpInfo();
        }
    }

    std::string app_file_name;
    std::vector<u8> app_file;
    if (argc - optind == 1) {
        app_file_name = argv[optind++];
        app_file = ReadBinaryFile(app_file_name);
    } else {
        ShowHelpInfo();
    }

    if (app_file.empty()) {
        printf("ERROR: Input file does not exist!\n");
        return 1;
    }

    if (memcmp(&app_file[NCCH_Header::OFFSET_MAGIC], "NCCH", 4) == 0) {
        NCCH ncch(&app_file[0], app_file.size());
        if (!DecryptNCCH(prsd, &ncch))
            return 1;
        WriteBinaryFile(ReplaceExtension(app_file_name, "cxi"), ncch.GetBuffer(), ncch.GetSize());
    } else if (memcmp(&app_file[NCSD_Header::OFFSET_MAGIC], "NCSD", 4) == 0) {
        NCSD ncsd(&app_file[0], app_file.size());
        if (!DecryptNCSD(prsd, &ncsd))
            return 1;
        WriteBinaryFile(ReplaceExtension(app_file_name, "cci"), ncsd.GetBuffer(), ncsd.GetSize());
    } else {
        printf("ERROR: Unsupported input file type!");
        return 1;
    }

    return 0;
}
