// Code that allows reading INI files into data structure
// unordered map <string, unordered map<string,string> >  from constructor
// 20220904 weg, started
// Was config.cpp in
// From https://codereview.stackexchange.com/questions/163251/ini-file-parser-in-c-take-2
// and earlier https://codereview.stackexchange.com/questions/127819/ini-file-parser-in-c 
// Original author https://codereview.stackexchange.com/users/7008/arcomber  2016
//
//  Usage: config cfg("filename");  cfg.get_value("section", "key"); Returns string.  
//
#include "configINI.h"

#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <unordered_map>

#include <cstdio>  // std::remove
#include <cassert>

using namespace std;

// trim leading white-spaces
static string& ltrim(string& s) {
    size_t startpos = s.find_first_not_of(" \t\r\n\v\f");
    if (string::npos != startpos)  {
        s = s.substr(startpos);
    }
    return s;
}

// trim trailing white-spaces
static string& rtrim(string& s) {
    size_t endpos = s.find_last_not_of(" \t\r\n\v\f");
    if (string::npos != endpos) {
        s = s.substr(0, endpos + 1);
    }
    return s;
}

config::config(const string& filename) {
        ifstream fstrm;
        fstrm.open(filename);

        if (!fstrm)
            //throw invalid_argument(filename + " could not be opened");
            // Exception handling has been disabled, so above flags an error.
            cout << "config::config: **Error, file " << filename << "could not be opened." << endl;

        parse(fstrm);
}

config::config(istream& strm) {
    parse(strm);
}

// default constructor, weg, 20230203
config::config() {

}

const config::sections config::get_sections() const {
    return sections_;
}

const string config::get_value(const string& sectionname, const string&keyname) const {

    auto sect = sections_.find(sectionname);
    if (sect != sections_.end()) {
        auto key = sect->second.find(keyname);
        if (key != sect->second.end()) {
            return key->second;
        }
    }

    return "";
}

void config::set_value(const string& sectionname, const string& keyname, const string& value) {

    auto sect = sections_.find(sectionname);

    if (sect == sections_.end()) {
        auto ref = sections_[sectionname];
        sect = sections_.find(sectionname);
    }

    assert(sect != sections_.end());

    auto key = sect->second.find(keyname);
    if (key != sect->second.end()) {
        key->second = value;
    }
    else {
        sect->second.insert(make_pair(keyname, value));
    }
}

bool config::save_changes(const string& filename) {
    bool success(false);

    // delete existing file - if exists
    remove(filename.c_str());
    // iterate thru sections_ saving data to a file
    string currentsectionname;
    ofstream fstrm;
    fstrm.open(filename);

    if (fstrm.good()) {
        for (auto heading : sections_) {
            fstrm << '[' << heading.first << "]\n";
            for (auto kvs : heading.second) {
                fstrm << kvs.first << '=' << kvs.second << '\n';
            }
            fstrm << '\n';
        }
        success = true;
    }

    return success;
}

string config::print_file(){
    //
    // iterate sections in ini file, below sec reference is iterator for cfg3
    string astring;
    for (auto& sec : sections_) {          
        // From stl_pair.h, line 188 ?!  sections_ is an unordered map of 
        // string and unordered map, see how it is used above.  weg
        //cout << sec.first << endl;
        astring += "[" + sec.first + "]\n";
        // iterate key value pairs in section
        for (auto& data : sec.second) {
            //cout << "\t" << data.first << "\t" << data.second << endl;
            astring += data.first + " = ";
            astring += data.second + "\n";
        }
    }    return astring;

}

void config::parse(istream& strm) {

    string currentsectionname;
    for (string line; getline(strm, line);)
    {
        // skip comments
        if (!line.empty() && (line[0] == ';' || line[0] == '#')) {
            // allow both ; and # comments at the start of a line
        }
        else if (line[0] == '[') {
            /* A "[section]" line */
            size_t end = line.find_first_of(']');
            if (end != string::npos) {

                // this is a new section so add it to hashtable
                currentsectionname = line.substr(1, end - 1);
                sections_[currentsectionname];
            }
            // else section has no closing ] char - ignore
        }
        else if (!line.empty()) {
            /* Not a comment, must be a name[=:]value pair */
            size_t end = line.find_first_of("=:");
            if (end != string::npos) {
                string name = line.substr(0, end);
                string value = line.substr(end + 1);
                ltrim(rtrim(name));
                ltrim(rtrim(value));

                sections_[currentsectionname].insert(make_pair(name, value));
            }
            //else no key value delimitter - ignore
        }
    } // for
}
