#include <bits/stdc++.h>
using namespace std;

#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define MAGENTA "\033[35m"

using sys_time = chrono::system_clock::time_point;

// (All structures and functions are the same as your version; only main/menu flushing/tie changed)

struct CycleEntry {
    string startDate;
    string endDate;
    int durationDays = 0;
    int cycleLength = 0;
    CycleEntry() = default;
    CycleEntry(string s, string e, int d, int c)
        : startDate(move(s)), endDate(move(e)), durationDays(d), cycleLength(c) {}
};

struct DailyLog { string date, symptoms, mood; };

struct Reminder {
    sys_time when;
    string message;
    
    bool operator==(const Reminder &o) const { return when == o.when && message == o.message; }
};

struct CompareReminder {
    bool operator()(const Reminder &a, const Reminder &b) const { return a.when > b.when; }
};

// trim helper
static inline string trim(const string &s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) ++a;
    while (b > a && isspace((unsigned char)s[b-1])) --b;
    return s.substr(a, b - a);
}

bool parseDate(const string &in, tm &out_tm) {
    out_tm = tm{};
    istringstream ss(in);
    ss >> get_time(&out_tm, "%Y-%m-%d");
    return !ss.fail();
}

bool stringToTimePoint(const string &s, sys_time &tp) {
    tm tm_ = {};
    if (!parseDate(s, tm_)) return false;
    tm_.tm_hour = tm_.tm_min = tm_.tm_sec = 0;
    time_t tt = mktime(&tm_);
    if (tt == -1) return false;
    tp = chrono::system_clock::from_time_t(tt);
    return true;
}

string timePointToString(const sys_time &tp) {
    time_t tt = chrono::system_clock::to_time_t(tp);
    tm buf = *localtime(&tt);
    ostringstream os;
    os << put_time(&buf, "%Y-%m-%d");
    return os.str();
}

int daysBetween(const string &d1, const string &d2) {
    sys_time t1, t2;
    if (!stringToTimePoint(d1, t1) || !stringToTimePoint(d2, t2)) return 0;
    time_t tt1 = chrono::system_clock::to_time_t(t1);
    time_t tt2 = chrono::system_clock::to_time_t(t2);
    long long secs = static_cast<long long>(tt2) - static_cast<long long>(tt1);
    return static_cast<int>(secs / (24LL * 3600LL));
}

string addDays(const string &dateStr, int days) {
    sys_time t;
    if (!stringToTimePoint(dateStr, t)) return "";
    t += chrono::seconds(24LL * 3600LL * days);
    return timePointToString(t);
}

int daysFromTodayTo(const string &dateStr) {
    sys_time now = chrono::system_clock::now();
    sys_time target;
    if (!stringToTimePoint(dateStr, target)) return 0;
    time_t ttNow = chrono::system_clock::to_time_t(now);
    time_t ttT = chrono::system_clock::to_time_t(target);
    long long secs = static_cast<long long>(ttT) - static_cast<long long>(ttNow);
    return static_cast<int>(secs / (24LL * 3600LL));
}

// ---------- PeriodTracker (identical to your previous fixed version) ----------
class PeriodTracker {
private:
    vector<CycleEntry> cycles;
    map<string, DailyLog> dailyLogs;
    stack<CycleEntry> undoStack;
    stack<CycleEntry> redoStack;
    priority_queue<Reminder, vector<Reminder>, CompareReminder> reminders;
    const string cyclesFile = "cycles.csv";
    const string logsFile = "daily_logs.csv";

    void loadData() {
        ifstream cf(cyclesFile);
        if (cf.is_open()) {
            string line;
            while (getline(cf, line)) {
                line = trim(line);
                if (line.empty()) continue;
                vector<string> parts;
                string cur;
                istringstream ss(line);
                while (getline(ss, cur, ',')) parts.push_back(trim(cur));
                if (parts.size() >= 4) {
                    try {
                        int d = stoi(parts[2]);
                        int c = stoi(parts[3]);
                        cycles.emplace_back(parts[0], parts[1], d, c);
                    } catch (...) { continue; }
                }
            }
        }
        ifstream lf(logsFile);
        if (lf.is_open()) {
            string line;
            while (getline(lf, line)) {
                line = trim(line);
                if (line.empty()) continue;
                vector<string> parts;
                string cur;
                istringstream ss(line);
                while (getline(ss, cur, ',')) parts.push_back(trim(cur));
                if (parts.size() >= 3) {
                    dailyLogs[parts[0]] = {parts[0], parts[1], parts[2]};
                }
            }
        }
        rebuildRemindersFromCycles();
    }

    void saveData() const {
        ofstream cf(cyclesFile, ios::trunc);
        for (const auto &c : cycles)
            cf << c.startDate << "," << c.endDate << "," << c.durationDays << "," << c.cycleLength << "\n";
        ofstream lf(logsFile, ios::trunc);
        for (const auto &p : dailyLogs) {
            string safeSymptoms = p.second.symptoms; replace(safeSymptoms.begin(), safeSymptoms.end(), ',', ';');
            string safeMood = p.second.mood; replace(safeMood.begin(), safeMood.end(), ',', ';');
            lf << p.first << "," << safeSymptoms << "," << safeMood << "\n";
        }
    }

    int averageCycleLength() const {
        int sum = 0, cnt = 0;
        for (const auto &c : cycles) if (c.cycleLength > 0) { sum += c.cycleLength; ++cnt; }
        return (cnt > 0) ? (sum / cnt) : 28;
    }

    void printHeader(const string &title) const {
        cout << BOLD << CYAN << "\n╔════════════════════════════════════════════════════════╗\n";
        cout << "  " << title << "\n";
        cout << "╚════════════════════════════════════════════════════════╝\n" << RESET;
    }

    void pushReminder(const Reminder &r) { reminders.push(r); }

    void cleanupPastReminders() {
        while (!reminders.empty()) {
            Reminder top = reminders.top();
            if (top.when < chrono::system_clock::now()) reminders.pop();
            else break;
        }
    }

public:
    PeriodTracker() { loadData(); }
    ~PeriodTracker() {}

    void addCycleFromUser() {
        printHeader("✨ ADD NEW CYCLE ENTRY ✨");
        string start, end;
        cout << "Enter START date (YYYY-MM-DD): ";
        cin >> start;
        cout << "Enter END date (YYYY-MM-DD): ";
        cin >> end;
        tm tm1 = {}, tm2 = {};
        if (!parseDate(start, tm1) || !parseDate(end, tm2)) {
            cout << RED << "❌ Invalid date format. Use YYYY-MM-DD." << RESET << "\n";
            return;
        }
        int duration = daysBetween(start, end);
        if (duration < 0) {
            cout << RED << "❌ End date must be after start date." << RESET << "\n";
            return;
        }
        int cycleLen = 0;
        if (!cycles.empty()) cycleLen = daysBetween(cycles.back().startDate, start);
        CycleEntry e(start, end, duration, cycleLen);
        cycles.push_back(e);
        undoStack.push(e);
        while (!redoStack.empty()) redoStack.pop();
        cout << GREEN << "✅ Cycle recorded: " << start << " -> " << end << RESET << "\n";
        cout << YELLOW << "Duration: " << duration << " days" << RESET << "\n";
        rebuildRemindersFromCycles();
    }

    void deleteCycleByStart() {
        printHeader("🗑️ DELETE CYCLE ENTRY (by START date) 🗑️");
        if (cycles.empty()) { cout << YELLOW << "No cycles to delete." << RESET << "\n"; return; }
        string target; cout << "Enter START date of cycle to delete (YYYY-MM-DD): "; cin >> target;
        auto it = find_if(cycles.begin(), cycles.end(), [&](const CycleEntry &c){ return c.startDate == target; });
        if (it == cycles.end()) { cout << RED << "Not found." << RESET << "\n"; return; }
        CycleEntry removed = *it; cycles.erase(it);
        undoStack.push(removed); while (!redoStack.empty()) redoStack.pop();
        cout << GREEN << "✅ Deleted cycle starting " << removed.startDate << RESET << "\n";
        rebuildRemindersFromCycles();
    }

    void undo() {
        printHeader("↶ UNDO (last cycle action)");
        if (undoStack.empty()) { cout << YELLOW << "Nothing to undo." << RESET << "\n"; return; }
        CycleEntry top = undoStack.top(); undoStack.pop();
        auto it = find_if(cycles.begin(), cycles.end(), [&](const CycleEntry &c){
            return c.startDate == top.startDate && c.endDate == top.endDate;
        });
        if (it != cycles.end()) {
            cycles.erase(it);
            cout << GREEN << "Undo: removed cycle starting " << top.startDate << RESET << "\n";
            redoStack.push(top);
        } else {
            cycles.push_back(top);
            cout << GREEN << "Undo: restored cycle starting " << top.startDate << RESET << "\n";
            redoStack.push(top);
        }
        rebuildRemindersFromCycles();
    }

    void redo() {
        printHeader("↷ REDO (re-apply last undone)");
        if (redoStack.empty()) { cout << YELLOW << "Nothing to redo." << RESET << "\n"; return; }
        CycleEntry top = redoStack.top(); redoStack.pop();
        auto it = find_if(cycles.begin(), cycles.end(), [&](const CycleEntry &c){
            return c.startDate == top.startDate && c.endDate == top.endDate;
        });
        if (it != cycles.end()) {
            cycles.erase(it);
            cout << GREEN << "Redo: removed cycle starting " << top.startDate << RESET << "\n";
            undoStack.push(top);
        } else {
            cycles.push_back(top);
            cout << GREEN << "Redo: restored cycle starting " << top.startDate << RESET << "\n";
            undoStack.push(top);
        }
        rebuildRemindersFromCycles();
    }

    void logDailySymptomFromUser() {
        printHeader("📝 LOG DAILY SYMPTOM & MOOD 📝");
        string date; cout << "Enter DATE (YYYY-MM-DD): "; cin >> date;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        string symptoms, mood;
        cout << "Enter SYMPTOMS: "; getline(cin, symptoms);
        cout << "Enter MOOD: "; getline(cin, mood);
        if (dailyLogs.count(date)) {
            if (!symptoms.empty()) {
                if (!dailyLogs[date].symptoms.empty()) dailyLogs[date].symptoms += "; ";
                dailyLogs[date].symptoms += symptoms;
            }
            if (!mood.empty()) dailyLogs[date].mood = mood;
        } else dailyLogs[date] = {date, symptoms, mood};
        cout << GREEN << "✅ Logged for " << date << RESET << "\n";
    }

    void displayCycles() const {
        printHeader("🩸 MENSTRUAL CYCLE HISTORY 🩸");
        if (cycles.empty()) { cout << YELLOW << "No cycles recorded yet." << RESET << "\n"; return; }
        cout << left << BOLD;
        cout << setw(12) << "START" << setw(12) << "END" << setw(8) << "DAYS" << setw(12) << "CYCLE_LEN" << "\n" << RESET;
        cout << "------------------------------------------------\n";
        for (const auto &c : cycles) {
            cout << setw(12) << c.startDate << setw(12) << c.endDate << setw(8) << c.durationDays
                 << setw(12) << (c.cycleLength > 0 ? to_string(c.cycleLength) : "N/A") << "\n";
        }
    }

    void displayDailyLogs() const {
        printHeader("📈 DAILY SYMPTOM LOGS & MOOD 📊");
        if (dailyLogs.empty()) { cout << YELLOW << "No logs yet." << RESET << "\n"; return; }
        cout << left << setw(12) << "DATE" << setw(40) << "SYMPTOMS" << "MOOD\n";
        cout << "----------------------------------------------------------------\n";
        for (const auto &p : dailyLogs)
            cout << setw(12) << p.first << setw(40) << p.second.symptoms << p.second.mood << "\n";
    }

    void predictNextPeriod() const {
        printHeader("🔮 NEXT PERIOD PREDICTION 🔮");
        if (cycles.empty()) { cout << YELLOW << "Add at least one cycle to predict." << RESET << "\n"; return; }
        int avgLen = averageCycleLength();
        string lastStart = cycles.back().startDate;
        string predictedNext = addDays(lastStart, avgLen);
        cout << CYAN << "Average cycle length: " << avgLen << " days" << RESET << "\n";
        cout << GREEN << "Next predicted period start: " << BOLD << predictedNext << RESET << "\n";
        int daysLeft = daysFromTodayTo(predictedNext);
        if (daysLeft >= 0) cout << YELLOW << "Days left until next period: " << daysLeft << RESET << "\n";
        else cout << YELLOW << "Predicted date is in the past by " << -daysLeft << " day(s)." << RESET << "\n";
    }

    void rebuildRemindersFromCycles() {
        while (!reminders.empty()) reminders.pop();
        if (!cycles.empty()) {
            int avgLen = averageCycleLength();
            string lastStart = cycles.back().startDate;
            string predicted = addDays(lastStart, avgLen);
            sys_time tp;
            if (stringToTimePoint(predicted, tp)) {
                pushReminder({tp, "Predicted next period: " + predicted});
            }
        }
    }

    void showReminders() {
        cleanupPastReminders();
        printHeader("⏰ UPCOMING REMINDERS ⏰");
        if (reminders.empty()) { cout << YELLOW << "No upcoming reminders." << RESET << "\n"; return; }
        auto copyPQ = reminders;
        int i = 1;
        while (!copyPQ.empty() && i <= 10) {
            Reminder r = copyPQ.top(); copyPQ.pop();
            string dateStr = timePointToString(r.when);
            int daysAway = daysFromTodayTo(dateStr);
            cout << i << ". " << r.message << " (Date: " << BOLD << dateStr << RESET << ", in " << daysAway << " day(s))\n";
            ++i;
        }
    }

    void addManualReminder() {
        printHeader("➕ ADD MANUAL REMINDER ➕");
        string date, msg; cout << "Enter date (YYYY-MM-DD): "; cin >> date;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Enter reminder message: "; getline(cin, msg);
        sys_time tp;
        if (!stringToTimePoint(date, tp)) { cout << RED << "Invalid date format." << RESET << "\n"; return; }
        pushReminder({tp, msg});
        cout << GREEN << "Reminder added for " << date << RESET << "\n";
    }

    void showAnalytics() const {
        printHeader("📊 ANALYTICS SUMMARY 📊");
        if (cycles.empty()) { cout << YELLOW << "No cycles to analyze." << RESET << "\n"; return; }
        int sumDur = 0, sumLen = 0, countLen = 0;
        int minDur = INT_MAX, maxDur = INT_MIN;
        int minLen = INT_MAX, maxLen = INT_MIN;
        for (const auto &c : cycles) {
            sumDur += c.durationDays;
            minDur = min(minDur, c.durationDays);
            maxDur = max(maxDur, c.durationDays);
            if (c.cycleLength > 0) {
                sumLen += c.cycleLength; ++countLen;
                minLen = min(minLen, c.cycleLength); maxLen = max(maxLen, c.cycleLength);
            }
        }
        double avgDur = double(sumDur) / cycles.size();
        cout << "Cycles recorded: " << cycles.size() << "\n";
        cout << fixed << setprecision(2);
        cout << "Duration (days) - Avg: " << avgDur << ", Min: " << minDur << ", Max: " << maxDur << "\n";
        if (countLen > 0) {
            double avgLen = double(sumLen) / countLen;
            cout << "Cycle length (days) - Avg: " << avgLen << ", Min: " << minLen << ", Max: " << maxLen << "\n";
        } else cout << "Cycle length data insufficient (need >=2 cycles to compute lengths).\n";
    }

    void saveAndExit() { printHeader("💾 SAVING & EXITING"); saveData(); cout << GREEN << "Data saved (cycles.csv, daily_logs.csv)." << RESET << "\n"; cout << "Goodbye! 👋\n"; }
};

// ---------------- Menu & main ----------------
void displayMenu() {
    cout << BOLD << "\n───────── 🌸 PERIOD TRACKER 🌸 ─────────\n" << RESET;
    cout << "1. Add New Cycle\n";
    cout << "2. Delete Cycle (by start date)\n";
    cout << "3. Undo (last add/delete)\n";
    cout << "4. Redo\n";
    cout << "5. Log Daily Symptom & Mood\n";
    cout << "6. View Cycle History\n";
    cout << "7. Predict Next Period (only date)\n";
    cout << "8. Reminders (show) / Add manual reminder\n";
    cout << "9. Analytics Summary\n";
    cout << "10. View Daily Logs\n";
    cout << "11. Save & Exit\n";
    cout << "---------------------------------------------\n";
    cout << "Enter choice (1-11): " << flush; // <-- flush so prompt shows before cin
}

int main() {
    ios::sync_with_stdio(false);
    // re-tie cin to cout so prompts flush automatically before input
    cin.tie(&cout);

    PeriodTracker tracker;
    bool running = true;
    while (running) {
        displayMenu();
        int choice;
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            choice = 0;
        }
        switch (choice) {
            case 1: tracker.addCycleFromUser(); break;
            case 2: tracker.deleteCycleByStart(); break;
            case 3: tracker.undo(); break;
            case 4: tracker.redo(); break;
            case 5: tracker.logDailySymptomFromUser(); break;
            case 6: tracker.displayCycles(); break;
            case 7: tracker.predictNextPeriod(); break;
            case 8: {
                cout << "a) Show reminders   b) Add manual reminder\nChoose (a/b): " << flush;
                char ch; cin >> ch;
                if (ch == 'a') tracker.showReminders();
                else if (ch == 'b') tracker.addManualReminder();
                else cout << RED << "Invalid option\n" << RESET;
                break;
            }
            case 9: tracker.showAnalytics(); break;
            case 10: tracker.displayDailyLogs(); break;
            case 11: tracker.saveAndExit(); running = false; break;
            default: cout << RED << "Invalid choice (1-11).\n" << RESET;
        }
    }
    return 0;
}
