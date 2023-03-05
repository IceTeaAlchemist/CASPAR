#ifndef CONFIGINI_H_
#define CONFIGINI_H_
// Header for config.cpp .
// 20220904 weg, started
// Was config.hpp in
// From https://codereview.stackexchange.com/questions/163251/ini-file-parser-in-c-take-2
// and earlier https://codereview.stackexchange.com/questions/127819/ini-file-parser-in-c 
// Original author https://codereview.stackexchange.com/users/7008/arcomber 2016
//
#include <string>
#include <istream>
#include <unordered_map>

using namespace std;

// class to encapsulate ini file style configuration text file
class config
{
public:
    // key=value pair within a section
    typedef pair<string, string> keyvalue;  // NOT USED??

    // [section1] - top level
    typedef unordered_map< string, unordered_map<string, string> > sections;

    // construct with a filename
    config(const string& filename);
    // construct with a stream
    config(istream& strm);
    // no copying allowed
    config(const config&) = delete;
    config& operator = (const config&) = delete;
    // default constructure, weg, 20230203
    config();

    // return a copy of whole structure
    const sections get_sections() const;
    // get a value
    const string get_value(const string& sectionname, const string& keyname) const;
    // set a value
    void set_value(const string& sectionname, const string& keyname, const string& value);
    // any changes will not be committed to underlying file until save_changes() is called
    bool save_changes(const string& filename);
    // standard way to iterate underlying container
    typedef sections::iterator iterator;
    iterator begin() { return sections_.begin(); }
    iterator end() { return sections_.end(); }
    // print the whole file as a string (weg)
    string print_file();

private:
    // parse stream into config data structure
    void parse(istream& strm);
    // top level of data structure - hash table
    sections sections_;
};

#endif // CONFIG_HPP_