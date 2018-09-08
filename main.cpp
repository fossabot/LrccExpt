/**
 * Copyright (C) 2018 Pandorym
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

using namespace std;

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
    int space;
    string line;
};

struct FileInfo {
    string masterPath;
    string xmpPath;
};

vector <string> split(const string &s, const string &seperator) {
    vector <string> result;
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

int main() {
    sqlite3pp::database db("/Users/pandorym/Pictures/Lightroom Library.lrlibrary/c77d608db1a44a4d9ab8deee035bcd79/Managed Catalog.mcat");

    system("mkdir imgFile");

    sqlite3pp::query qry(db, "SELECT * FROM docs");

    QTY qty;
    vector <DOC> docs;

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

    cout << "Total: " << qty.exist
         << " (" << qty.total << "-" << qty.deleted << ")" << endl;


    vector <FileInfo> fileInfos;

    map<string, int> suffix;

    for (auto &doc : docs) {

        vector <Line_AND_Space> LS;

        std::istringstream f(doc.annotation);

        string line;
        while (getline(f, line)) {
            Line_AND_Space currentLine;

            int space = 0;
            for (; line[space] == '\t'; space++);

            long separatorIndex = line.find(" = ");
            if (separatorIndex != -1) {
                line = line.replace(separatorIndex, 3, ": ");

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

        FileInfo fileInfo;

        auto masterPath = jsonAnnotation["_localOnly"]["files"]["original"]["master"]["relativePath"].asString();

        if (!masterPath.empty() && !doc.deleted) {

            cout << "Parse: " << doc.localDocId << " ";

            string masterName = getName(masterPath);
            string masterSuffix = getSuffix(masterPath);
            transform(masterSuffix.begin(), masterSuffix.end(), masterSuffix.begin(), ::toupper);
            suffix[masterSuffix]++;

            cout << doc.deleted << " " << doc.hasConflicts << " \"/" + masterPath << "\"";


            ifstream src;
            ofstream dst;

            src.open("/" + masterPath, ios::binary);
            dst.open("./imgFile/" + masterName, ios::binary);

            if (src.fail()) cout << " Error 1: Fail to open the source file.";
            else if (dst.fail()) cout << " Error 2: Fail to create the new file.";
            else dst << src.rdbuf();

            src.close();
            dst.close();


            cout << " Ok" << endl;


            auto xmpPath = jsonAnnotation["_localOnly"]["files"]["original"]["xmp"]["path"].asString();

            if (!xmpPath.empty()) {
                cout << "        \"" << xmpPath << "\" ";

                string xmpName = split(masterName, ".").front() + '.' +getSuffix(xmpPath);

                cout << "\"./imgFile/" + xmpName + "\"";

                ifstream src_xmp;
                ofstream dst_xmp;

                src_xmp.open("/" + xmpPath, ios::binary);
                dst_xmp.open("./imgFile/" + xmpName, ios::binary);

                if (src_xmp.fail()) cout << " Error 1: Fail to open the source file.";
                else if (dst_xmp.fail()) cout << " Error 2: Fail to create the new file.";
                else dst_xmp << src_xmp.rdbuf();

                src_xmp.close();
                dst_xmp.close();

                cout << " Ok" << endl;
            }
        }
    }

    cout << endl;
    for (auto &i : suffix) {
        cout << i.first << ": " << i.second << endl;
    }

    cout << "end" << endl;

}
