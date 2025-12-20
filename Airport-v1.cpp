// flight_tool.cpp
// Compile: g++ -std=c++17 flight_tool.cpp -o flight_tool
#include <iostream>
#include <string>
#include <regex>
#include <vector>
#include <sstream>
#include <fstream>
#include <optional>
#include <iomanip>
#include <filesystem>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
namespace fs = std::filesystem;

// Store recent valid entries so kids can see their work again
const fs::path kLatestFile = "latestValues.txt";

/*
const fs::path kLatestFile = "PRO/latestValues.txt";
// Ensure the directory exists
void ensureDirectoryExists() {
    fs::path dir = kLatestFile.parent_path();
    if (!dir.empty() && !fs::exists(dir)) {
        fs::create_directories(dir);
    }
}
*/

// -------------------- Terminal helpers (cross-platform) --------------------
static bool supports_colors = true;

void enableVirtualTerminalOnWindows() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) { supports_colors = false; return; }
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) { supports_colors = false; return; }
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode)) { supports_colors = false; return; }
#endif
}
void color(const string &code) {
    if (!supports_colors) return;
    cout << code;
}
void red()   { color("\033[1;31m"); }
void green() { color("\033[1;32m"); }
void cyan()  { color("\033[1;36m"); }
void yellow(){ color("\033[1;33m"); }
void resetc(){ color("\033[0m"); }

void clearScreen() {
#ifdef _WIN32
    // Use WinAPI for safer clearing
    HANDLE hStd = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStd == INVALID_HANDLE_VALUE) {
        system("cls");
        return;
    }
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hStd, &csbi);
    DWORD cellCount = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD count;
    COORD home = {0, 0};
    FillConsoleOutputCharacter(hStd, ' ', cellCount, home, &count);
    FillConsoleOutputAttribute(hStd, csbi.wAttributes, cellCount, home, &count);
    SetConsoleCursorPosition(hStd, home);
#else
    // ANSI escape clear
    cout << "\033[2J\033[H";
#endif
}

// -------------------- Data model --------------------
struct Record {
    int time_h = 0, time_m = 0, time_s = 0;
    string flight;
    string computer;
};

struct ValidationResult {
    bool ok;
    string message;
};

bool validateFlightId(const string &flt, string &msg) {
    if (flt.empty()) { msg = "flight id missing"; return false; }
    if (!isalpha(static_cast<unsigned char>(flt[0]))) { msg = "flight id must start with a letter"; return false; }
    if (flt.size() > 10) { msg = "flight id too long (max 10)"; return false; }
    return true;
}

bool validateComputerId(const string &comp, string &msg) {
    if (comp.size() != 3) { msg = "computer id must be exactly 3 characters"; return false; }
    for (char ch: comp) if (!isalnum(static_cast<unsigned char>(ch))) { msg = "computer id must be letters/numbers"; return false; }
    char b = toupper(static_cast<unsigned char>(comp[1])), c = toupper(static_cast<unsigned char>(comp[2]));
    if (b=='I' || b=='O' || c=='I' || c=='O') { msg = "avoid letter I or O in last two spots"; return false; }
    return true;
}

// Parse and validate HHMMSS-like numeric input or HH:MM:SS
optional<Record> parseRecordFromLine(const string &line, ValidationResult &vr) {
    // Accept formats:
    //  1) HHMMSS FLTID COMP
    //  2) HH:MM:SS,FLTID,COMP
    //  3) whitespace separated
    string s = line;
    // Trim front/back
    auto ltrim = [](string &str){ str.erase(str.begin(), find_if(str.begin(), str.end(), [](int ch){ return !isspace(ch); })); };
    auto rtrim = [](string &str){ str.erase(find_if(str.rbegin(), str.rend(), [](int ch){ return !isspace(ch); }).base(), str.end()); };
    ltrim(s); rtrim(s);
    if (s.empty()) { vr = {false, "empty line"}; return {}; }

    // Replace comma with space for simple tokenization
    string mod;
    for (char c: s) mod += (c == ',' ? ' ' : c);

    istringstream iss(mod);
    string timeToken, flt, comp;
    if (!(iss >> timeToken >> flt >> comp)) {
        vr = {false, "expected 3 pieces: time flight computer"};
        return {};
    }

    // Normalize and validate time
    // accept HHMMSS or HH:MM:SS
    regex r1(R"(^([0-9]{2}):([0-9]{2}):([0-9]{2})$)");
    regex r2(R"(^([0-9]{6})$)");
    smatch m;
    Record rec;
    if (regex_match(timeToken, m, r1)) {
        rec.time_h = stoi(m[1].str()); rec.time_m = stoi(m[2].str()); rec.time_s = stoi(m[3].str());
    } else if (regex_match(timeToken, m, r2)) {
        string t = m[1].str();
        rec.time_h = stoi(t.substr(0,2)); rec.time_m = stoi(t.substr(2,2)); rec.time_s = stoi(t.substr(4,2));
    } else {
        vr = {false, "time should look like 09:25:30 or 092530"};
        return {};
    }
    auto valid_time = [](int hh,int mm,int ss){
        return hh>=0 && hh<=23 && mm>=0 && mm<=59 && ss>=0 && ss<=59;
    };
    if (!valid_time(rec.time_h, rec.time_m, rec.time_s)) {
        vr = {false, "time out of range"};
        return {};
    }

    string msg;
    if (!validateFlightId(flt, msg)) { vr = {false, msg}; return {}; }
    if (!validateComputerId(comp, msg)) { vr = {false, msg}; return {}; }

    // assign validated fields
    rec.flight = flt;
    rec.computer = comp;
    vr = {true, "OK"};
    return rec;
}

string formatTime(const Record &r) {
    ostringstream out;
    out << setfill('0') << setw(2) << r.time_h << ":" << setw(2) << r.time_m << ":" << setw(2) << r.time_s;
    return out.str();
}

// -------------------- Utilities --------------------
string promptLine(const string &prompt) {
    cout << prompt;
    string res;
    getline(cin, res);
    return res;
}

void pauseForKey() {
    cout << "\nPress Enter to continue...";
    string tmp;
    getline(cin, tmp);
}

// -------------------- Features --------------------
void showHeader() {
    cyan();
    cout << "========================================\n";
    cout << "   FLIGHT CHECKER KID FRIENDLY MODE   \n";
    cout << "========================================\n";
    resetc();
}

void showQuickRules() {
    cout << "\nQuick rules and examples:\n";
    cout << " 1) Time: 09:25:30 or 092530 (24-hour clock).\n";
    cout << " 2) Flight: start with a letter, up to 10 letters/numbers.\n";
    cout << " 3) Computer: 3 letters/numbers, avoid I or O in last two spots.\n";
    cout << "Sample line: 09:25:30 ABC123 XYZ\n";
    cout << "Saved good answers go to latestValues.txt so you can see them later.\n";
}

optional<int> askNumber(const string &label, int lo, int hi) {
    while (true) {
        cout << label << " (" << lo << "-" << hi << ", or 'q' to cancel): ";
        string text; getline(cin, text);
        if (text == "q" || text == "Q") return {};
        if (text.empty()) continue;
        try {
            int v = stoi(text);
            if (v < lo || v > hi) { red(); cout << "Please stay between " << lo << " and " << hi << ".\n"; resetc(); continue; }
            return v;
        } catch (...) {
            red(); cout << "Type numbers only.\n"; resetc();
        }
    }
}

optional<Record> kidFriendlyWizard() {
    cout << "\nGuided mode, we will build the line together.\n";
    auto hh = askNumber("Hour", 0, 23);
    if (!hh) return {};
    auto mm = askNumber("Minute", 0, 59);
    if (!mm) return {};
    auto ss = askNumber("Second", 0, 59);
    if (!ss) return {};

    string flt;
    string msg;
    while (true) {
        cout << "Flight ID (start with a letter): ";
        getline(cin, flt);
        if (flt == "q" || flt == "Q") return {};
        if (validateFlightId(flt, msg)) break;
        red(); cout << "Try again: " << msg << "\n"; resetc();
    }

    string comp;
    while (true) {
        cout << "Computer ID (3 letters/numbers, no I or O in last two): ";
        getline(cin, comp);
        if (comp == "q" || comp == "Q") return {};
        if (validateComputerId(comp, msg)) break;
        red(); cout << "Try again: " << msg << "\n"; resetc();
    }

    Record r;
    r.time_h = *hh; r.time_m = *mm; r.time_s = *ss;
    r.flight = flt; r.computer = comp;
    return r;
}

void appendRecordToLatest(const Record &r) {
    ofstream out(kLatestFile, ios::app);
    if (!out) {
        red(); cout << "Could not open " << kLatestFile << " to save the entry.\n"; resetc();
        return;
    }
    out << formatTime(r) << ' ' << r.flight << ' ' << r.computer << '\n';
}

void displayLatestFile() {
    cout << "\nStored values (" << kLatestFile << "):\n";
    if (!fs::exists(kLatestFile)) {
        yellow(); cout << "  No saved answers yet. Try option 1 or 2 first.\n"; resetc();
        return;
    }
    ifstream in(kLatestFile);
    if (!in) { red(); cout << "  Cannot open " << kLatestFile << "\n"; resetc(); return; }
    string line; size_t lineNo = 0;
    while (getline(in, line)) {
        ++lineNo;
        cout << "  " << lineNo << ") " << line << "\n";
    }
    if (lineNo == 0) {
        yellow(); cout << "  (file is empty)\n"; resetc();
    }
}

void interactiveSingleEntry() {
    cout << "\nEnter a single record (try: 09:25:30 ABC123 XYZ)\n";
    string line = promptLine("Record > ");
    ValidationResult vr;
    auto rec = parseRecordFromLine(line, vr);
    if (!rec) {
        red();
        cout << "Invalid: " << vr.message << "\n";
        resetc();
        showQuickRules();
        return;
    }
    green();
    cout << "Valid record:\n";
    resetc();
    cout << "  Time: " << formatTime(*rec) << "\n";
    cout << "  Flight: " << rec->flight << "\n";
    cout << "  Computer: " << rec->computer << "\n";
}

void analyzeFile(const fs::path &filepath) {
    if (!fs::exists(filepath)) {
        red(); cout << "File not found: " << filepath << "\n"; resetc();
        return;
    }
    ifstream fin(filepath);
    if (!fin) {
        red(); cout << "Cannot open file: " << filepath << "\n"; resetc(); return;
    }

    cout << "Analyzing file: " << filepath << "\n";
    string line;
    size_t lineNo = 0;
    size_t okCount = 0, badCount = 0;
    vector<pair<size_t,string>> errors;
    vector<Record> goodRecords;

    while (getline(fin, line)) {
        ++lineNo;
        ValidationResult vr;
        auto rec = parseRecordFromLine(line, vr);
        if (!rec) {
            badCount++;
            errors.emplace_back(lineNo, vr.message + " -- \"" + line + "\"");
        } else {
            okCount++;
            goodRecords.push_back(*rec);
        }
    }

    cout << "\nSummary:\n";
    green(); cout << "  OK: " << okCount << "\n"; resetc();
    if (badCount) { red(); cout << "  Invalid: " << badCount << "\n"; resetc(); }
    cout << "\nDetailed errors (first 10):\n";
    for (size_t i=0;i<errors.size() && i<10;++i) {
        cout << "  Line " << errors[i].first << ": " << errors[i].second << "\n";
    }

    if (badCount) {
        yellow();
        cout << "\nTip: run Guided Mode from the main menu to practice building a correct line.\n";
        resetc();
    }

    // Offer to export good records to CSV
    if (!goodRecords.empty()) {
        cout << "\nExport valid records to CSV? (y/N): ";
        string ans; getline(cin, ans);
        if (!ans.empty() && (ans[0]=='y' || ans[0]=='Y')) {
            fs::path outp = filepath;
            outp.replace_extension(".valid.csv");
            ofstream out(outp);
            out << "time,flight,computer\n";
            for (auto &r : goodRecords) {
                out << formatTime(r) << ',' << r.flight << ',' << r.computer << '\n';
            }
            cout << "Wrote " << goodRecords.size() << " records to " << outp << "\n";
        }
    }
}

void displayStored(const optional<Record> &r) {
    cout << "\nStored values:\n";
    if (!r) {
        yellow(); cout << "  (no stored record)\n"; resetc();
        return;
    }
    cout << "  Time: " << formatTime(*r) << "\n";
    cout << "  Flight: " << r->flight << "\n";
    cout << "  Computer: " << r->computer << "\n";
}

// -------------------- Main menu --------------------
int main() {
    // Keep stdio sync/ties default so prompts flush before input on Windows consoles
    enableVirtualTerminalOnWindows();
    ensureDirectoryExists();

    optional<Record> stored;
    while (true) {
        clearScreen();
        showHeader();
        cout << "[1] Quick check (one-line input)\n";
        cout << "[2] Guided mode (kid friendly)\n";
        cout << "[3] Display stored record\n";
        cout << "[4] Read & display file (raw)\n";
        cout << "[5] Analyze file (batch validation + export)\n";
        cout << "[6] Help / examples\n";
        cout << "[7] Exit\n\n";
        cout << "Choose option: ";
        cout.flush();
        string opt;
        getline(cin, opt);
        if (opt.empty()) continue;

        if (opt == "1") {
            cout << "\nEnter record now (or type 'cancel'):\n";
            string line = promptLine("> ");
            if (line == "cancel") continue;
            ValidationResult vr;
            auto rec = parseRecordFromLine(line, vr);
            if (!rec) {
                red(); cout << "Invalid: " << vr.message << "\n"; resetc();
                pauseForKey();
            } else {
                green(); cout << "Record valid.\n"; resetc();
                stored = *rec;
                appendRecordToLatest(*rec);
                cout << "Saved to " << kLatestFile << "\n";
                pauseForKey();
            }
        } else if (opt == "2") {
            auto rec = kidFriendlyWizard();
            if (!rec) {
                yellow(); cout << "Canceled guided entry.\n"; resetc();
            } else {
                green(); cout << "Nice! Record is ready.\n"; resetc();
                cout << "It looks like: " << formatTime(*rec) << " " << rec->flight << " " << rec->computer << "\n";
                stored = *rec;
                appendRecordToLatest(*rec);
                cout << "Saved to " << kLatestFile << "\n";
            }
            pauseForKey();
        } else if (opt == "3") {
            displayLatestFile();
            pauseForKey();
        } else if (opt == "4") {
            string path = promptLine("Enter path (default data.txt): ");
            if (path.empty()) path = "data.txt";
            ifstream f(path);
            if (!f) { red(); cout << "Cannot open: " << path << "\n"; resetc(); pauseForKey(); }
            else {
                cout << "\n---- File content ----\n";
                string ln; size_t i=0;
                while (getline(f, ln) && i<200) { cout << ln << "\n"; ++i; }
                if (!f.eof()) cout << "... (file long; only first 200 lines shown)\n";
                pauseForKey();
            }
        } else if (opt == "5") {
            string path = promptLine("Enter path to analyze (default data.txt): ");
            if (path.empty()) path = "data.txt";
            analyzeFile(path);
            pauseForKey();
        } else if (opt == "6") {
            showQuickRules();
            cout << "\nUse Guided Mode if you are unsure. Type 'q' while answering to cancel.\n";
            pauseForKey();
        } else if (opt == "7") {
            cout << "Goodbye!\n";
            break;
        } else {
            cout << "Unknown option.\n";
            pauseForKey();
        }
    }
    return 0;
}
