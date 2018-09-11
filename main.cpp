/**
 * Copyright (C) 2018 Pandorym <i@Pandorym.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

#include <iostream>
#include <sstream>
#include <string>
#include "string.h"
#include <algorithm>
#include <iterator>
#include <vector>
#include <map>
#include <cctype>
#include <fstream>
#include "json/json.h"
#include "sqlite3pp/sqlite3pp.h"
#include <unistd.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/types.h>
#include <regex>

using namespace std;

string DEFAULT_LRLIB;

/* quantity */
struct QTY {
    int total = 0;
    int exist = 0;
    int deleted = 0;
};

struct DOC {
    string fullDocId, annotation;
    int localDocId, winningRevSequence, mostRecentRevSequence, deleted, hasConflicts;
};

struct Line_AND_Space {
    int space{};
    string line;
};

struct FileInfo {
    string masterPath;
    string xmpPath;
    int localDocId;
    int xmpSidecarExists;
    bool deleted = false;
    Json::Value annotation;
};

struct User {
    string name;
    string email;
    string id;
    string cat;
};

vector<string> split(const string &s, const string &seperator) {
    vector<string> result;
    typedef string::size_type string_size;
    string_size i = 0;

    while (i != s.size()) {
        int flag = 0;
        while (i != s.size() && flag == 0) {
            flag = 1;
            for (char x : seperator) {
                if (s[i] == x) {
                    ++i;
                    flag = 0;
                    break;
                }
            }
        }

        flag = 0;
        string_size j = i;
        while (j != s.size() && flag == 0) {
            for (char x : seperator) {
                if (s[j] == x) {
                    flag = 1;
                    break;
                }
            }
            if (flag == 0) ++j;
        }
        if (i != j) {
            result.push_back(s.substr(i, j - i));
            i = j;
        }
    }
    return result;
}

string getName(string path) {
    return split(path, "/").back();
}

string getSuffix(string path) {
    return split(path, ".").back();
}


string getCurrentTerminalUesr() {
    return getpwuid(getuid())->pw_name;
}

vector<string> getUserdb_Path(string userdir_path) {
    vector<string> userdb_paths;

    regex r("oz-user-[^.\s]{32}");

    dirent *ptr;
    DIR *userDir = opendir(userdir_path.c_str());

    while ((ptr = readdir(userDir)) != nullptr) {
        string filename = ptr->d_name;

        if (regex_match(filename, r))
            userdb_paths.push_back(userdir_path + filename);
    }
    closedir(userDir);

    return userdb_paths;
}

Json::Value parseAnnotation(string rawAnnotation) {
    vector<Line_AND_Space> LS;
    istringstream f(rawAnnotation);

    string line;
    while (getline(f, line)) {
        Line_AND_Space currentLine;

        int space = 0;
        for (; line[space] == '\t'; space++);

        long separatorIndex = line.find(" = ");
        if (separatorIndex != -1) {
            line = line.replace(static_cast<unsigned long>(separatorIndex), 3, ": ");

            for (long i = space; i < separatorIndex; i++)
                if (line[i] == '\"') line[i] = '\'';

            line.insert(line.begin() + separatorIndex, '\"');
            line.insert(line.begin() + space, '\"');
        }

        currentLine.line = line;
        currentLine.space = space;

        if (LS.size() > 1) {

            Line_AND_Space lastLine = LS.back();
            if (lastLine.space > currentLine.space) {
                lastLine.line.pop_back();

                LS.pop_back();
                LS.push_back(lastLine);
            }
        }

        LS.push_back(currentLine);

    }

    string strAnnotation;
    for (auto &i : LS) {
        strAnnotation += i.line + '\n';
    }

    Json::Reader JsonParser;
    Json::Value jsonAnnotation;

    if (!JsonParser.parse(strAnnotation, jsonAnnotation)) {
        // cout << "parse error" << endl;
        // cout << JsonParser.getStructuredErrors()[0].message << endl;
    }
    // cout << jsonAnnotation << endl;

    return jsonAnnotation;
}

/***
 * copy file from src_path to dst_path
 * @param src_path
 * @param dst_path
 * @return 0: succ; 1: src error; 2: dst error;
 */
int copyFile(string src_path, string dst_path) {
    ifstream src(src_path, ios::binary);
    ofstream dst(dst_path, ios::binary);

    if (src.good() && dst.good())
        dst << src.rdbuf();

    src.close();
    dst.close();

    if (src.fail()) return 1;
    if (dst.fail()) return 2;
    return 0;
}

User getUser(const string &userdb_path) {
    User user;

    sqlite3pp::database db(userdb_path.c_str());

    sqlite3pp::query *qry;
    string annotation_str;

    qry = new sqlite3pp::query{db, "SELECT annotation FROM docs"};
    tie(annotation_str) = (*qry->begin()).get_columns < char const* > (0);
    user.cat = parseAnnotation(annotation_str)["_localOnly"]["managedCatalog"]["id"].asString();

    qry = new sqlite3pp::query{db, "SELECT content FROM revs WHERE current = 1"};
    tie(annotation_str) = (*qry->begin()).get_columns < char const* > (0);
    auto content = parseAnnotation(annotation_str);
    user.id = content["id"].asString();
    user.name = content["full_name"].asString();
    user.email = content["email"].asString();

    return user;

}

vector<User> getUsers(string lrlib_path) {
    string userdir_path = lrlib_path + "/user/";
    vector<string> userdb_paths = getUserdb_Path(userdir_path);

    vector<User> Users;
    Users.reserve(userdb_paths.size());
    for (const auto &userdb_path : userdb_paths) {
        Users.push_back(getUser(userdb_path));
    }

    return Users;
}

int main() {

    cout << "Copyright (C) 2018, Pandorym (www.pandorym.com), released under the AGPLv3 license.\n"
         << '\n'
         << "  The program includes source code from: \n"
         << "    1. SQLite3, Public domain;\n"
         << "    2. sqlite3pp, released under the MIT license\n"
         << "         Copyright (c) 2015 Wongoo Lee (iwongu at gmail dot com);\n"
         << "    3. JsonCpp, Public Domain.\n"
         << '\n'
         << "------------------------------------------------------------------------------------------" << endl;

    DEFAULT_LRLIB = "/Users/" + getCurrentTerminalUesr() + "/Pictures/Lightroom Library.lrlibrary";

    cout << "\nLrLibrary path: " << DEFAULT_LRLIB << "\n\n";

    auto users = getUsers(DEFAULT_LRLIB);

    int i(0);
    for (auto &user : users) {
        cout << ++i << ". " << user.cat << " " << user.id << " \"" << user.name << "\" " << user.email << endl;
    }


    string strUserNo;
    cout << "\e[36mselect the user (Enter NO., default is \e[0m1\e[36m): \e[0m";
    getline(cin, strUserNo);

    int userNo;
    if (strUserNo.empty()) userNo = 0;
    else userNo = stoi(strUserNo) - 1;

    string mcat_path = DEFAULT_LRLIB + '/' + users[userNo].cat + "/Managed Catalog.mcat";

    sqlite3pp::database db(mcat_path.c_str());
    cout << "\n> open mcat: " + mcat_path << "\n\n";

    sqlite3pp::query qry(db, "SELECT * FROM docs");

    QTY qty;
    vector<DOC> docs;

    for (auto v : qry) {
        DOC doc;
        v.getter() >> doc.localDocId
                   >> doc.fullDocId
                   >> doc.winningRevSequence
                   >> doc.mostRecentRevSequence
                   >> doc.deleted
                   >> doc.hasConflicts
                   >> doc.annotation;
        docs.push_back(doc);

        doc.deleted ? qty.deleted++ : qty.exist++;
        qty.total++;
    }


    vector<FileInfo> fileInfos;

    map<string, int> suffix;

    for (auto &doc : docs) {
        FileInfo fileInfo;

        auto jsonAnnotation = parseAnnotation(doc.annotation);


        fileInfo.localDocId = doc.localDocId;
        fileInfo.masterPath = jsonAnnotation["_localOnly"]["files"]["original"]["master"]["relativePath"].asString();;
        fileInfo.deleted = static_cast<bool>(doc.deleted);
        fileInfo.xmpSidecarExists = jsonAnnotation["_localOnly"]["xmpSidecarExists"].asBool();

        if (fileInfo.xmpSidecarExists) {
            fileInfo.xmpPath = jsonAnnotation["_localOnly"]["files"]["original"]["xmp"]["path"].asString();
        }

        fileInfos.push_back(fileInfo);

        if (!fileInfo.masterPath.empty() && !fileInfo.deleted) {
            string masterSuffix = getSuffix(fileInfo.masterPath);
            transform(masterSuffix.begin(), masterSuffix.end(), masterSuffix.begin(), ::toupper);
            suffix[masterSuffix]++;
        }
    }

    cout << "------------------------------" << endl;
    cout << "Total: " << qty.exist
         << " (" << qty.total << "-" << qty.deleted << ")" << endl;
    unsigned long siffixWidth = 5;
    for (auto &i : suffix) {
        if (i.first.length() > siffixWidth) siffixWidth = i.first.length();
    }
    for (auto &i : suffix) {
        cout.width(siffixWidth);
        cout.setf(ios::left);
        cout.fill(' ');
        cout << i.first;
        cout << ": " << i.second << endl;
    }
    cout << "------------------------------" << endl;

    string consoleLine;
    cout << "\e[36mAre you sure? (\e[0myes\e[36m/no\e[0m): ";
    getline(cin, consoleLine);

    if (consoleLine == "exit" || consoleLine == "quit" || consoleLine == "no") {
        cout << "> exit LrccExpt" << endl;
        return 0;
    }

    system("mkdir imgFile");

    for (auto &fileInfo : fileInfos) {

        cout << "\e[2J" << "\e[0;0H";
        cout << "Parse: " << fileInfo.localDocId << "/" << fileInfos.size() << " ";

        if (!fileInfo.masterPath.empty() && !fileInfo.deleted) {

            cout << "Exporting files";

            string masterName = getName(fileInfo.masterPath);


            cout << "\n"
                 << "  master: from \"/" + fileInfo.masterPath + "\"\n"
                 << "            => \"./imgFile/" + masterName + "\""
                 << flush;

            switch (copyFile("/" + fileInfo.masterPath, "./imgFile/" + masterName)) {
                case 1:
                    cout << " \e[31m×\n    Error : Fail to open the source file.\e[0m" << endl;
                    break;
                case 2:
                    cout << " \e[31m×\n    Error: Fail to create the new file.\e[0m" << endl;
                    break;
                default:
                    cout << " \e[32m√\e[0m" << endl;
                    break;
            }


            if (fileInfo.xmpSidecarExists) {

                string xmpName = split(masterName, ".").front() + '.' + getSuffix(fileInfo.xmpPath);

                cout << "  xmp   : from \"" + fileInfo.xmpPath + "\"\n"
                     << "            => \"./imgFile/" + xmpName + "\""
                     << flush;

                switch (copyFile(fileInfo.xmpPath, "./imgFile/" + xmpName)) {
                    case 1:
                        cout << " \e[31m×\n    Error : Fail to open the source file.\e[0m" << endl;
                        break;
                    case 2:
                        cout << " \e[31m×\n    Error: Fail to create the new file.\e[0m" << endl;
                        break;
                    default:
                        cout << " \e[32m√\e[0m" << endl;
                        break;
                }
            }
        } else {
            if (fileInfo.deleted)
                cout << "file was deleted." << endl;
            else if (fileInfo.masterPath.empty())
                cout << "master path does not exist." << endl;
        }
    }

    cout << endl;
    cout << "done." << endl;
    cout << "open the directory: " << "./imgFile\n" << endl;

}
