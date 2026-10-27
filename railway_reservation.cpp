#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// Demonstration credentials only. Do not use these for a real service.
const string ADMIN_USERNAME = "admin";
const string ADMIN_PASSWORD = "admin123";
const string TRAINS_FILE = "trains.txt";
const string BOOKINGS_FILE = "bookings.txt";

string trim(string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

string cleanField(string s) {
    // Pipe is the storage delimiter; replace it to keep records readable.
    for (char &c : s) {
        if (c == '|') c = '/';
        if (c == '\n' || c == '\r') c = ' ';
    }
    return trim(s);
}

string lowerCopy(string s) {
    transform(s.begin(), s.end(), s.begin(),
              [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return s;
}

string readLine(const string &prompt) {
    cout << prompt;
    string value;
    if (!getline(cin, value)) {
        cout << "\nInput closed. Exiting safely.\n";
        exit(0);
    }
    return trim(value);
}

int readInt(const string &prompt, int minimum, int maximum) {
    while (true) {
        string s = readLine(prompt);
        stringstream ss(s);
        int value;
        char extra;
        if ((ss >> value) && !(ss >> extra) && value >= minimum && value <= maximum)
            return value;
        cout << "Invalid input. Enter a whole number from " << minimum
             << " to " << maximum << ".\n";
    }
}

double readDouble(const string &prompt, double minimum, double maximum) {
    while (true) {
        string s = readLine(prompt);
        stringstream ss(s);
        double value;
        char extra;
        if ((ss >> value) && !(ss >> extra) && value >= minimum &&
            value <= maximum && value == value)
            return value;
        cout << "Invalid input. Enter a number from " << minimum
             << " to " << maximum << ".\n";
    }
}

bool askYesNo(const string &prompt) {
    while (true) {
        string answer = lowerCopy(readLine(prompt + " (y/n): "));
        if (answer == "y" || answer == "yes") return true;
        if (answer == "n" || answer == "no") return false;
        cout << "Please enter y or n.\n";
    }
}

struct Train {
    int number = 0;
    string name, source, destination, departure, arrival;
    int capacity = 0;
    int availableSeats = 0;
    double fare = 0.0;
};

struct Passenger {
    string name, gender;
    int age = 0;
};

struct Booking {
    string pnr;
    int trainNumber = 0;
    Passenger passenger;
    int seats = 0;
    double totalFare = 0.0;
    bool cancelled = false;
    string bookedAt;
};

class RailwayReservationSystem {
private:
    vector<Train> trains;
    vector<Booking> bookings;
    vector<string> sessionPNRs;
    mt19937_64 rng{random_device{}()};

    string currentTimestamp() const {
        time_t now = time(nullptr);
        tm localTime{};
#ifdef _WIN32
        localtime_s(&localTime, &now);
#else
        localtime_r(&now, &localTime);
#endif
        ostringstream out;
        out << put_time(&localTime, "%Y-%m-%d %H:%M:%S");
        return out.str();
    }

    Train* findTrain(int number) {
        for (auto &train : trains)
            if (train.number == number) return &train;
        return nullptr;
    }

    const Train* findTrain(int number) const {
        for (const auto &train : trains)
            if (train.number == number) return &train;
        return nullptr;
    }

    Booking* findBooking(const string &pnr) {
        for (auto &booking : bookings)
            if (booking.pnr == pnr) return &booking;
        return nullptr;
    }

    bool pnrExists(const string &pnr) const {
        for (const auto &booking : bookings)
            if (booking.pnr == pnr) return true;
        return false;
    }

    string generatePNR() {
        // Random numeric PNR; check persisted records to avoid collisions.
        for (int attempt = 0; attempt < 10000; ++attempt) {
            ostringstream out;
            out << "PNR" << setw(9) << setfill('0')
                << (rng() % 1000000000ULL);
            string candidate = out.str();
            if (!pnrExists(candidate)) return candidate;
        }
        // Practically unreachable fallback.
        return "PNR" + to_string(static_cast<unsigned long long>(time(nullptr))) +
               to_string(bookings.size());
    }

    static bool parseInt(const string &s, int &value) {
        stringstream ss(s);
        char extra;
        return (ss >> value) && !(ss >> extra);
    }

    static bool parseDouble(const string &s, double &value) {
        stringstream ss(s);
        char extra;
        return (ss >> value) && !(ss >> extra) && value == value;
    }

    void seedTrainsIfEmpty() {
        if (!trains.empty()) return;
        trains.push_back({12001, "Deccan Express", "Pune", "Mumbai", "07:15", "10:35", 120, 120, 450.0});
        trains.push_back({11029, "Koyna Express", "Mumbai", "Pune", "08:40", "12:50", 100, 100, 380.0});
        trains.push_back({12115, "Siddheshwar Express", "Solapur", "Mumbai", "22:10", "06:45", 150, 150, 620.0});
        trains.push_back({11040, "Maharashtra Express", "Kolhapur", "Gondia", "14:30", "09:20", 180, 180, 710.0});
        saveTrains();
    }

    void loadTrains() {
        ifstream file(TRAINS_FILE);
        if (!file) return; // Missing file is normal on first launch.
        string line;
        while (getline(file, line)) {
            stringstream ss(line);
            vector<string> f;
            string part;
            while (getline(ss, part, '|')) f.push_back(part);
            if (f.size() != 9) {
                cerr << "Warning: skipped malformed train record.\n";
                continue;
            }
            Train t;
            if (!parseInt(f[0], t.number) || !parseInt(f[6], t.capacity) ||
                !parseInt(f[7], t.availableSeats) || !parseDouble(f[8], t.fare) ||
                t.number <= 0 || t.capacity < 0 || t.availableSeats < 0 ||
                t.availableSeats > t.capacity || t.fare < 0) {
                cerr << "Warning: skipped invalid train record.\n";
                continue;
            }
            t.name = f[1]; t.source = f[2]; t.destination = f[3];
            t.departure = f[4]; t.arrival = f[5];
            if (!findTrain(t.number)) trains.push_back(t);
        }
    }

    void loadBookings() {
        ifstream file(BOOKINGS_FILE);
        if (!file) return;
        string line;
        while (getline(file, line)) {
            stringstream ss(line);
            vector<string> f;
            string part;
            while (getline(ss, part, '|')) f.push_back(part);
            if (f.size() != 9) {
                cerr << "Warning: skipped malformed booking record.\n";
                continue;
            }
            Booking b;
            int cancelledInt = 0;
            if (!parseInt(f[1], b.trainNumber) || !parseInt(f[4], b.passenger.age) ||
                !parseInt(f[5], b.seats) || !parseDouble(f[6], b.totalFare) ||
                !parseInt(f[7], cancelledInt) || (cancelledInt != 0 && cancelledInt != 1) ||
                b.trainNumber <= 0 || b.passenger.age < 1 || b.passenger.age > 120 ||
                b.seats < 1 || b.totalFare < 0) {
                cerr << "Warning: skipped invalid booking record.\n";
                continue;
            }
            b.pnr = f[0]; b.passenger.name = f[2]; b.passenger.gender = f[3];
            b.cancelled = cancelledInt == 1; b.bookedAt = f[8];
            if (!pnrExists(b.pnr)) bookings.push_back(b);
        }
    }

    bool writeRecords(const string &filename, const vector<string> &lines) {
        // Write to a temporary file first, then replace the destination.
        const string tempName = filename + ".tmp";
        {
            ofstream out(tempName, ios::trunc);
            if (!out) {
                cerr << "Error: cannot write " << tempName << ".\n";
                return false;
            }
            for (const auto &line : lines) out << line << '\n';
            if (!out) {
                cerr << "Error: failed while writing " << tempName << ".\n";
                return false;
            }
        }
        // Standard C++ has no fully portable atomic replace operation.
        // Rename after removing the old file; the temporary file protects
        // against most write failures, but keep backups for recovery.
        const string backupName = filename + ".bak";
        remove(backupName.c_str());
        ifstream old(filename, ios::binary);
        if (old) {
            old.close();
            if (rename(filename.c_str(), backupName.c_str()) != 0) {
                cerr << "Error: cannot back up " << filename << ".\n";
                remove(tempName.c_str());
                return false;
            }
        }
        if (rename(tempName.c_str(), filename.c_str()) != 0) {
            cerr << "Error: cannot replace " << filename << "; restoring backup if available.\n";
            rename(backupName.c_str(), filename.c_str());
            remove(tempName.c_str());
            return false;
        }
        remove(backupName.c_str());
        return true;
    }

    bool saveTrains() {
        vector<string> lines;
        for (const auto &t : trains) {
            ostringstream out;
            out << t.number << '|' << cleanField(t.name) << '|'
                << cleanField(t.source) << '|' << cleanField(t.destination) << '|'
                << cleanField(t.departure) << '|' << cleanField(t.arrival) << '|'
                << t.capacity << '|' << t.availableSeats << '|'
                << fixed << setprecision(2) << t.fare;
            lines.push_back(out.str());
        }
        return writeRecords(TRAINS_FILE, lines);
    }

    bool saveBookings() {
        vector<string> lines;
        for (const auto &b : bookings) {
            ostringstream out;
            out << cleanField(b.pnr) << '|' << b.trainNumber << '|'
                << cleanField(b.passenger.name) << '|'
                << cleanField(b.passenger.gender) << '|' << b.passenger.age << '|'
                << b.seats << '|' << fixed << setprecision(2) << b.totalFare << '|'
                << (b.cancelled ? 1 : 0) << '|' << cleanField(b.bookedAt);
            lines.push_back(out.str());
        }
        return writeRecords(BOOKINGS_FILE, lines);
    }

    void printTrainHeader() const {
        cout << left << setw(8) << "Number" << setw(25) << "Train name"
             << setw(16) << "Source" << setw(16) << "Destination"
             << setw(10) << "Depart" << setw(10) << "Arrive"
             << right << setw(8) << "Seats" << setw(12) << "Fare\n";
        cout << string(105, '-') << '\n';
    }

    void printTrain(const Train &t) const {
        cout << left << setw(8) << t.number << setw(25) << t.name.substr(0, 24)
             << setw(16) << t.source.substr(0, 15)
             << setw(16) << t.destination.substr(0, 15)
             << setw(10) << t.departure << setw(10) << t.arrival
             << right << setw(8) << t.availableSeats << setw(12)
             << fixed << setprecision(2) << t.fare << '\n';
    }

    void printBooking(const Booking &b) const {
        const Train *t = findTrain(b.trainNumber);
        cout << "\nPNR: " << b.pnr << "\nStatus: "
             << (b.cancelled ? "CANCELLED" : "CONFIRMED")
             << "\nPassenger: " << b.passenger.name
             << "\nAge / Gender: " << b.passenger.age << " / " << b.passenger.gender
             << "\nTrain number: " << b.trainNumber;
        if (t) cout << "\nTrain: " << t->name << "\nRoute: " << t->source
                    << " -> " << t->destination << "\nDeparture / Arrival: "
                    << t->departure << " / " << t->arrival;
        cout << "\nSeats: " << b.seats << "\nTotal fare: Rs. "
             << fixed << setprecision(2) << b.totalFare
             << "\nBooked at: " << b.bookedAt << "\n";
    }

    void viewAllTrains() const {
        if (trains.empty()) { cout << "No trains are available.\n"; return; }
        printTrainHeader();
        for (const auto &t : trains) printTrain(t);
    }

    void searchTrains() const {
        string source = lowerCopy(readLine("Enter source station: "));
        string destination = lowerCopy(readLine("Enter destination station: "));
        bool found = false;
        for (const auto &t : trains) {
            if (lowerCopy(t.source) == source &&
                lowerCopy(t.destination) == destination) {
                if (!found) printTrainHeader();
                printTrain(t);
                found = true;
            }
        }
        if (!found) cout << "No direct trains found for that route.\n";
    }

    void bookTicket() {
        viewAllTrains();
        if (trains.empty()) return;
        int number = readInt("Enter train number: ", 1, numeric_limits<int>::max());
        Train *train = findTrain(number);
        if (!train) { cout << "Train not found.\n"; return; }

        string name = cleanField(readLine("Passenger full name: "));
        if (name.empty()) { cout << "Name cannot be empty.\n"; return; }
        int age = readInt("Passenger age (1-120): ", 1, 120);
        string gender;
        while (true) {
            gender = cleanField(readLine("Gender (Male/Female/Other): "));
            string g = lowerCopy(gender);
            if (g == "male" || g == "female" || g == "other") {
                gender = (g == "male" ? "Male" : g == "female" ? "Female" : "Other");
                break;
            }
            cout << "Please enter Male, Female, or Other.\n";
        }
        int seats = readInt("Number of seats to book: ", 1, 1000);
        if (seats > train->availableSeats) {
            cout << "Only " << train->availableSeats << " seat(s) are available.\n";
            return;
        }

        Booking b;
        b.pnr = generatePNR();
        b.trainNumber = number;
        b.passenger = {name, gender, age};
        b.seats = seats;
        b.totalFare = train->fare * seats;
        b.cancelled = false;
        b.bookedAt = currentTimestamp();

        train->availableSeats -= seats;
        bookings.push_back(b);
        if (!saveTrains() || !saveBookings()) {
            // Roll back in-memory state if persistence fails.
            train->availableSeats += seats;
            bookings.pop_back();
            saveTrains();
            cout << "Booking was not saved. Please check file permissions and try again.\n";
            return;
        }
        sessionPNRs.push_back(b.pnr);
        cout << "\n========== BOOKING CONFIRMED ==========\n";
        printBooking(b);
        cout << "=======================================\n";
    }

    void checkPNR() const {
        string pnr = readLine("Enter PNR: ");
        for (const auto &b : bookings) {
            if (b.pnr == pnr) { printBooking(b); return; }
        }
        cout << "No booking found for that PNR.\n";
    }

    void cancelTicket() {
        string pnr = readLine("Enter PNR to cancel: ");
        Booking *booking = findBooking(pnr);
        if (!booking) { cout << "PNR not found.\n"; return; }
        if (booking->cancelled) { cout << "This booking is already cancelled.\n"; return; }
        printBooking(*booking);
        if (!askYesNo("Confirm cancellation")) {
            cout << "Cancellation aborted.\n";
            return;
        }
        Train *train = findTrain(booking->trainNumber);
        if (!train) {
            cout << "Cancellation failed: associated train record is missing.\n";
            return;
        }
        booking->cancelled = true;
        train->availableSeats = min(train->capacity, train->availableSeats + booking->seats);
        if (!saveTrains() || !saveBookings()) {
            booking->cancelled = false;
            train->availableSeats -= booking->seats;
            saveTrains();
            cout << "Cancellation could not be saved. No cancellation was applied.\n";
            return;
        }
        cout << "Ticket cancelled successfully. Seats have been restored.\n";
    }

    void viewSessionBookings() const {
        bool found = false;
        for (const string &pnr : sessionPNRs) {
            for (const auto &b : bookings) {
                if (b.pnr == pnr) {
                    printBooking(b);
                    found = true;
                    break;
                }
            }
        }
        if (!found) cout << "No bookings have been made in this session.\n";
    }

    void addTrain() {
        Train t;
        t.number = readInt("Train number: ", 1, numeric_limits<int>::max());
        if (findTrain(t.number)) { cout << "A train with that number already exists.\n"; return; }
        t.name = cleanField(readLine("Train name: "));
        t.source = cleanField(readLine("Source station: "));
        t.destination = cleanField(readLine("Destination station: "));
        t.departure = cleanField(readLine("Departure time (e.g. 08:30): "));
        t.arrival = cleanField(readLine("Arrival time (e.g. 13:45): "));
        if (t.name.empty() || t.source.empty() || t.destination.empty() ||
            t.departure.empty() || t.arrival.empty() ||
            lowerCopy(t.source) == lowerCopy(t.destination)) {
            cout << "Invalid train details. Fields must be filled and route stations must differ.\n";
            return;
        }
        t.capacity = readInt("Seat capacity: ", 1, 1000000);
        t.availableSeats = t.capacity;
        t.fare = readDouble("Fare per seat (Rs.): ", 0.0, 100000000.0);
        trains.push_back(t);
        if (!saveTrains()) {
            trains.pop_back();
            cout << "Train could not be saved.\n";
            return;
        }
        cout << "Train added successfully.\n";
    }

    void searchTrainByNumber() const {
        int number = readInt("Enter train number: ", 1, numeric_limits<int>::max());
        const Train *t = findTrain(number);
        if (!t) { cout << "Train not found.\n"; return; }
        printTrainHeader();
        printTrain(*t);
    }

    void updateTrain() {
        int number = readInt("Enter train number to update: ", 1, numeric_limits<int>::max());
        Train *t = findTrain(number);
        if (!t) { cout << "Train not found.\n"; return; }

        Train old = *t;
        cout << "1. Name\n2. Source and destination\n3. Departure and arrival\n"
             << "4. Fare\n5. Seat capacity\n";
        int choice = readInt("Choose field: ", 1, 5);
        switch (choice) {
            case 1: {
                string value = cleanField(readLine("New train name: "));
                if (value.empty()) { cout << "Name cannot be empty.\n"; return; }
                t->name = value;
                break;
            }
            case 2: {
                string source = cleanField(readLine("New source: "));
                string destination = cleanField(readLine("New destination: "));
                if (source.empty() || destination.empty() ||
                    lowerCopy(source) == lowerCopy(destination)) {
                    cout << "Invalid route.\n"; return;
                }
                t->source = source; t->destination = destination;
                break;
            }
            case 3: {
                string dep = cleanField(readLine("New departure time: "));
                string arr = cleanField(readLine("New arrival time: "));
                if (dep.empty() || arr.empty()) { cout << "Times cannot be empty.\n"; return; }
                t->departure = dep; t->arrival = arr;
                break;
            }
            case 4:
                t->fare = readDouble("New fare per seat (Rs.): ", 0.0, 100000000.0);
                break;
            case 5: {
                int activeSeats = 0;
                for (const auto &b : bookings)
                    if (b.trainNumber == number && !b.cancelled) activeSeats += b.seats;
                int capacity = readInt("New total capacity (must cover active bookings): ",
                                       max(1, activeSeats), 1000000);
                t->capacity = capacity;
                t->availableSeats = capacity - activeSeats;
                break;
            }
        }
        if (!saveTrains()) {
            *t = old;
            cout << "Update could not be saved; old values restored in memory.\n";
        } else cout << "Train updated successfully.\n";
    }

    void deleteTrain() {
        int number = readInt("Enter train number to delete: ", 1, numeric_limits<int>::max());
        auto it = find_if(trains.begin(), trains.end(),
                          [number](const Train &t) { return t.number == number; });
        if (it == trains.end()) { cout << "Train not found.\n"; return; }
        for (const auto &b : bookings) {
            if (b.trainNumber == number && !b.cancelled) {
                cout << "Cannot delete: this train has active bookings.\n";
                return;
            }
        }
        if (!askYesNo("Delete train " + it->name)) {
            cout << "Deletion aborted.\n";
            return;
        }
        size_t index = static_cast<size_t>(distance(trains.begin(), it));
        Train removed = *it;
        trains.erase(it);
        if (!saveTrains()) {
            trains.insert(trains.begin() + static_cast<long>(index), removed);
            cout << "Train could not be deleted from storage.\n";
        } else cout << "Train deleted successfully. Historical bookings are preserved.\n";
    }

    void viewAllReservations() const {
        if (bookings.empty()) { cout << "No reservations found.\n"; return; }
        cout << left << setw(16) << "PNR" << setw(10) << "Train"
             << setw(24) << "Passenger" << setw(8) << "Seats"
             << setw(14) << "Fare" << "Status\n";
        cout << string(82, '-') << '\n';
        for (const auto &b : bookings) {
            cout << left << setw(16) << b.pnr << setw(10) << b.trainNumber
                 << setw(24) << b.passenger.name.substr(0, 23) << setw(8) << b.seats
                 << setw(14) << fixed << setprecision(2) << b.totalFare
                 << (b.cancelled ? "Cancelled" : "Active") << '\n';
        }
    }

    void showStatistics() const {
        size_t active = 0, cancelled = 0;
        double revenue = 0.0;
        for (const auto &b : bookings) {
            if (b.cancelled) ++cancelled;
            else { ++active; revenue += b.totalFare; }
        }
        cout << "\n========== RESERVATION STATISTICS ==========\n"
             << "Total bookings (including cancelled): " << bookings.size() << '\n'
             << "Active bookings: " << active << '\n'
             << "Cancelled bookings: " << cancelled << '\n'
             << "Estimated revenue from active bookings: Rs. "
             << fixed << setprecision(2) << revenue << '\n'
             << "============================================\n";
    }

    bool adminLogin() const {
        cout << "\n========== ADMIN LOGIN ==========\n";
        string username = readLine("Username: ");
        string password = readLine("Password: ");
        if (username == ADMIN_USERNAME && password == ADMIN_PASSWORD) {
            cout << "Login successful.\n";
            return true;
        }
        cout << "Invalid username or password.\n";
        return false;
    }

    void userMenu() {
        while (true) {
            cout << "\n========== USER MENU ==========\n"
                 << "1. View all trains\n2. Search trains by route\n"
                 << "3. Book tickets\n4. Check ticket status (PNR)\n"
                 << "5. Cancel booking\n6. View this session's bookings\n"
                 << "0. Back to main menu\n";
            int choice = readInt("Choose an option: ", 0, 6);
            switch (choice) {
                case 1: viewAllTrains(); break;
                case 2: searchTrains(); break;
                case 3: bookTicket(); break;
                case 4: checkPNR(); break;
                case 5: cancelTicket(); break;
                case 6: viewSessionBookings(); break;
                case 0: return;
            }
        }
    }

    void adminMenu() {
        if (!adminLogin()) return;
        while (true) {
            cout << "\n========== ADMIN MENU ==========\n"
                 << "1. Add train\n2. View all trains\n3. Search train by number\n"
                 << "4. Update train\n5. Delete train\n6. View all reservations\n"
                 << "7. View statistics and revenue\n0. Log out\n";
            int choice = readInt("Choose an option: ", 0, 7);
            switch (choice) {
                case 1: addTrain(); break;
                case 2: viewAllTrains(); break;
                case 3: searchTrainByNumber(); break;
                case 4: updateTrain(); break;
                case 5: deleteTrain(); break;
                case 6: viewAllReservations(); break;
                case 7: showStatistics(); break;
                case 0: return;
            }
        }
    }

public:
    RailwayReservationSystem() {
        loadTrains();
        loadBookings();
        seedTrainsIfEmpty();

        // Reconcile available seats from active persisted bookings to avoid
        // seat-count drift after a partial write or manual file edit.
        for (auto &t : trains) {
            long long occupied = 0;
            for (const auto &b : bookings)
                if (b.trainNumber == t.number && !b.cancelled) occupied += b.seats;
            if (occupied > t.capacity) {
                cerr << "Warning: active bookings exceed capacity for train "
                     << t.number << ". Availability set to zero.\n";
                t.availableSeats = 0;
            } else {
                t.availableSeats = t.capacity - static_cast<int>(occupied);
            }
        }
        saveTrains();
    }

    void run() {
        cout << "====================================================\n"
             << "          RAILWAY RESERVATION SYSTEM\n"
             << "====================================================\n"
             << "Welcome! Manage train schedules and ticket bookings.\n";
        while (true) {
            cout << "\n=============== MAIN MENU ===============\n"
                 << "1. User module\n2. Admin module\n0. Exit\n";
            int choice = readInt("Choose an option: ", 0, 2);
            if (choice == 1) userMenu();
            else if (choice == 2) adminMenu();
            else {
                cout << "Thank you for using the Railway Reservation System. Goodbye!\n";
                break;
            }
        }
    }
};

int main() {
    try {
        RailwayReservationSystem system;
        system.run();
    } catch (const exception &e) {
        cerr << "Unexpected error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
