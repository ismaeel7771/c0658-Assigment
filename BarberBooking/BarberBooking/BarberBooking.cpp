// BarberBooking.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>                         // Provides console input/output streams
#include <string>                           // Provides std::string
#include <vector>                           // Provides std::vector container
#include <unordered_map>                    // Provides std::unordered_map hash table
#include <unordered_set>                    // Provides std::unordered_set hash set
#include <queue>                            // Provides std::queue and std::priority_queue
#include <list>                             // Provides std::list doubly-linked list
#include <set>                              // Provides std::set ordered tree set
#include <iomanip>                          // Provides formatting helpers (setw etc.)
#include <limits>                           // Provides numeric limits for input cleaning

using namespace std;                        // Avoids repeating std:: prefix for standard library names

// -----------------------------
// Domain models (Data Structure Classes)
// -----------------------------

class Customer {
private:
    int id;                                 // Stores the unique identifier for the customer
    string name;                            // Stores the customer's name for display and identification
    string phone;                           // Stores the customer's phone number for contact/lookup
public:
    Customer() : id(0), name(""), phone("") {}              // Default constructor used by containers when needed
    Customer(int id, const string& name, const string& phone)
        : id(id), name(name), phone(phone) {
    }               // Initialises all customer fields in one place

    int getId() const { return id; }                        // Returns the customer ID for linking to appointments
    const string& getName() const { return name; }          // Returns the customer name without copying
    const string& getPhone() const { return phone; }        // Returns the customer phone without copying
};

class Barber {
private:
    int id;                                 // Stores the unique identifier for the barber
    string name;                            // Stores the barber’s name shown in schedules
public:
    Barber() : id(0), name("") {}                           // Default constructor for container compatibility
    Barber(int id, const string& name) : id(id), name(name) {} // Initialises barber fields

    int getId() const { return id; }                        // Returns barber ID for bookings and schedules
    const string& getName() const { return name; }          // Returns barber name for display
};

class Service {
private:
    int id;                                 // Stores a unique identifier for the service type
    string name;                            // Stores the service name (e.g., "Skin Fade")
    int durationMinutes;                    // Stores service duration to support realistic modelling
    double price;                           // Stores service price for billing summaries
public:
    Service() : id(0), name(""), durationMinutes(0), price(0.0) {} // Default constructor
    Service(int id, const string& name, int durationMinutes, double price)
        : id(id), name(name), durationMinutes(durationMinutes), price(price) {
    } // Initialises service data

    int getId() const { return id; }                        // Returns service ID for appointment references
    const string& getName() const { return name; }          // Returns service name for display
    int getDurationMinutes() const { return durationMinutes; } // Returns service duration for information
    double getPrice() const { return price; }               // Returns price for summaries
};

enum class ApptStatus {
    Scheduled,                           // Appointment is active and will occur
    Cancelled,                           // Appointment was cancelled and should not be served
    Completed                            // Appointment has been served
};

class Appointment {
private:
    int appointmentId;                     // Unique appointment identifier for cancel/reschedule
    int customerId;                        // Links appointment to a Customer via ID
    int barberId;                          // Links appointment to a Barber via ID
    int serviceId;                         // Links appointment to a Service via ID
    string dateTime;                       // Stores ISO-like "YYYY-MM-DD HH:MM" for ordering and display
    bool isVip;                            // Marks VIP customers for priority handling
    ApptStatus status;                     // Stores current status for scheduling logic
public:
    Appointment()
        : appointmentId(0), customerId(0), barberId(0), serviceId(0),
        dateTime(""), isVip(false), status(ApptStatus::Scheduled) {
    }          // Default constructor

    Appointment(int appointmentId, int customerId, int barberId, int serviceId,
        const string& dateTime, bool isVip)
        : appointmentId(appointmentId), customerId(customerId), barberId(barberId),
        serviceId(serviceId), dateTime(dateTime), isVip(isVip),
        status(ApptStatus::Scheduled) {
    }                                      // Sets initial status to Scheduled

    int getAppointmentId() const { return appointmentId; }                      // Returns appointment ID
    int getCustomerId() const { return customerId; }                            // Returns customer link
    int getBarberId() const { return barberId; }                                // Returns barber link
    int getServiceId() const { return serviceId; }                              // Returns service link
    const string& getDateTime() const { return dateTime; }                      // Returns scheduled time
    bool getIsVip() const { return isVip; }                                     // Returns VIP flag
    ApptStatus getStatus() const { return status; }                             // Returns appointment status

    void setStatus(ApptStatus newStatus) { status = newStatus; }                // Updates status (cancel/complete)
    void setDateTime(const string& newDateTime) { dateTime = newDateTime; }     // Updates schedule time (reschedule)
    void setBarberId(int newBarberId) { barberId = newBarberId; }               // Updates barber assignment
};

// -----------------------------
// BookingSystem (main data-structure interaction)
// -----------------------------

class BookingSystem {
private:
    unordered_map<int, Customer> customers;               // Hash map for fast customer retrieval by ID
    unordered_map<int, Barber> barbers;                   // Hash map for fast barber retrieval by ID
    vector<Service> services;                             // Contiguous list of services for iteration/display
    unordered_map<int, Appointment> appointmentsById;     // Hash map for O(1) appointment access by appointmentId
    list<int> activeAppointmentIds;                       // Linked list of active appointment IDs for easy removal
    unordered_set<string> bookedSlots;                    // Hash set to prevent double-booking of barber|time slots
    unordered_map<int, set<string>> availableSlotsByBarber; // Map barberId -> ordered set of available slots
    queue<int> walkInQueue;                               // FIFO queue storing customer IDs for walk-ins

    int nextCustomerId;                                   // Generates unique customer IDs
    int nextBarberId;                                     // Generates unique barber IDs
    int nextServiceId;                                    // Generates unique service IDs
    int nextAppointmentId;                                // Generates unique appointment IDs

    // Comparator used by the priority queue to decide which appointment is "next"
    struct UpcomingComparator {
        BookingSystem* system;                            // Pointer back to BookingSystem to read appointment details
        UpcomingComparator(BookingSystem* system) : system(system) {} // Stores the system pointer for comparisons

        bool operator()(int aId, int bId) const {          // Returns true if aId should come AFTER bId in priority
            const Appointment& a = system->appointmentsById.at(aId);   // Reads appointment A from the map
            const Appointment& b = system->appointmentsById.at(bId);   // Reads appointment B from the map

            if (a.getDateTime() != b.getDateTime()) {      // Primary sort key: earliest datetime should come first
                return a.getDateTime() > b.getDateTime();  // In priority_queue, "greater" means lower priority
            }
            if (a.getIsVip() != b.getIsVip()) {            // Secondary key: VIP should be served first if same time
                return a.getIsVip() < b.getIsVip();        // VIP=true gets higher priority than VIP=false
            }
            return a.getAppointmentId() > b.getAppointmentId(); // Tertiary key: smaller ID first for stability
        }
    };

    // Priority queue stores appointment IDs, ordered by time then VIP then ID
    priority_queue<int, vector<int>, UpcomingComparator> upcomingQueue;

    string makeSlotKey(int barberId, const string& dateTime) const {
        return to_string(barberId) + "|" + dateTime;       // Creates a unique string key for barber-time collision checks
    }

    void rebuildUpcomingQueue() {
        upcomingQueue = priority_queue<int, vector<int>, UpcomingComparator>(UpcomingComparator(this)); // Reset queue
        for (const auto& pair : appointmentsById) {        // Iterates all appointments in the hash map
            const Appointment& appt = pair.second;         // Extracts appointment object for checking
            if (appt.getStatus() == ApptStatus::Scheduled) { // Only scheduled appointments should be served
                upcomingQueue.push(appt.getAppointmentId()); // Pushes appointment ID into priority queue ordering
            }
        }
    }

public:
    BookingSystem()
        : nextCustomerId(1),                               // Start customer IDs at 1 for readability
        nextBarberId(1),                                 // Start barber IDs at 1
        nextServiceId(1),                                // Start service IDs at 1
        nextAppointmentId(1),                            // Start appointment IDs at 1
        upcomingQueue(UpcomingComparator(this)) {
    }        // Construct priority queue with comparator linked to this

// ------- Customer management -------

    int addCustomer(const string& name, const string& phone) {
        int id = nextCustomerId++;                         // Generates a unique customer ID
        customers.emplace(id, Customer(id, name, phone));   // Inserts customer into hash map for O(1) average access
        return id;                                         // Returns ID to allow immediate booking
    }

    void listCustomers() const {
        cout << "\nCustomers:\n";                           // Prints a heading for clarity in output
        for (const auto& pair : customers) {                // Iterates through all customers in the map
            const Customer& c = pair.second;                // Gets the Customer object from the pair
            cout << "ID: " << c.getId()                     // Displays customer ID
                << " | Name: " << c.getName()              // Displays customer name
                << " | Phone: " << c.getPhone() << "\n";   // Displays customer phone
        }
    }

    bool customerExists(int customerId) const {
        return customers.find(customerId) != customers.end(); // Checks presence in hash map in O(1) average time
    }

    // ------- Walk-in management -------

    void addWalkIn(int customerId) {
        walkInQueue.push(customerId);                      // Adds walk-in customer ID to FIFO queue
    }

    void serveWalkInIfAny() {
        if (walkInQueue.empty()) {                         // Checks if any walk-ins exist
            cout << "No walk-ins waiting.\n";               // Reports that there is nothing to serve
            return;                                        // Exits method early
        }
        int customerId = walkInQueue.front();              // Reads the next walk-in customer ID
        walkInQueue.pop();                                 // Removes that customer from the queue
        cout << "Serving walk-in customer ID: " << customerId << "\n"; // Displays who is being served
    }

    // ------- Barber management -------

    int addBarber(const string& name) {
        int id = nextBarberId++;                            // Generates a unique barber ID
        barbers.emplace(id, Barber(id, name));              // Inserts barber into hash map for fast lookup
        availableSlotsByBarber.emplace(id, set<string>());  // Creates an empty ordered set of slots for that barber
        return id;                                          // Returns barber ID for slot creation and booking
    }

    void listBarbers() const {
        cout << "\nBarbers:\n";                              // Prints heading
        for (const auto& pair : barbers) {                   // Iterates barbers map
            const Barber& b = pair.second;                   // Access barber object
            cout << "ID: " << b.getId()                      // Print ID
                << " | Name: " << b.getName() << "\n";      // Print name
        }
    }

    bool barberExists(int barberId) const {
        return barbers.find(barberId) != barbers.end();      // Returns true if barber key exists
    }

    // ------- Service management -------

    int addService(const string& name, int durationMinutes, double price) {
        int id = nextServiceId++;                            // Generates a unique service ID
        services.push_back(Service(id, name, durationMinutes, price)); // Adds service to vector for easy display
        return id;                                           // Returns service ID for bookings
    }

    void listServices() const {
        cout << "\nServices:\n";                              // Prints heading
        for (const auto& s : services) {                      // Iterates through the vector of services
            cout << "ID: " << s.getId()                       // Displays service ID
                << " | " << s.getName()                      // Displays service name
                << " | " << s.getDurationMinutes() << " mins"// Displays duration
                << " | £" << fixed << setprecision(2) << s.getPrice() // Displays price
                << "\n";                                     // Line break
        }
    }

    bool serviceExists(int serviceId) const {
        for (const auto& s : services) {                      // Linear search through services vector
            if (s.getId() == serviceId) return true;          // Returns true if matching ID is found
        }
        return false;                                         // Returns false if no match was found
    }

    // ------- Slot management (calendar-style) -------

    bool addAvailableSlot(int barberId, const string& dateTime) {
        if (!barberExists(barberId)) {                        // Validates barber exists before adding slot
            return false;                                     // Returns failure if barber ID invalid
        }
        availableSlotsByBarber[barberId].insert(dateTime);    // Inserts slot into ordered set (O(log n))
        return true;                                          // Returns success
    }

    void listAvailableSlots(int barberId) const {
        auto it = availableSlotsByBarber.find(barberId);      // Finds the barber's slot set in the map
        if (it == availableSlotsByBarber.end()) {             // Checks if barber has a slot set
            cout << "Barber not found.\n";                    // Reports missing barber
            return;                                           // Exits early
        }
        cout << "\nAvailable slots for Barber " << barberId << ":\n"; // Displays heading
        if (it->second.empty()) {                             // Checks if there are no available slots
            cout << "(none)\n";                               // Prints a friendly message
            return;                                           // Exits early
        }
        for (const auto& slot : it->second) {                 // Iterates ordered slots in ascending time order
            cout << "- " << slot << "\n";                     // Prints each slot
        }
    }

    // ------- Appointment management -------

    bool bookAppointment(int customerId, int barberId, int serviceId, const string& dateTime, bool isVip) {
        if (!customerExists(customerId)) {                    // Ensures customer exists before booking
            cout << "Customer not found.\n";                  // Explains why booking failed
            return false;                                     // Signals failure
        }
        if (!barberExists(barberId)) {                        // Ensures barber exists before booking
            cout << "Barber not found.\n";                    // Explains failure
            return false;                                     // Signals failure
        }
        if (!serviceExists(serviceId)) {                      // Ensures service exists before booking
            cout << "Service not found.\n";                   // Explains failure
            return false;                                     // Signals failure
        }

        // Check the barber's available slots first (calendar constraint)
        auto& slotSet = availableSlotsByBarber[barberId];     // Retrieves the set of slots for that barber
        if (slotSet.find(dateTime) == slotSet.end()) {        // Verifies requested time exists as an available slot
            cout << "That time is not available for this barber.\n"; // Explains availability rule
            return false;                                     // Signals failure
        }

        string slotKey = makeSlotKey(barberId, dateTime);     // Builds unique collision key for barber-time pair
        if (bookedSlots.find(slotKey) != bookedSlots.end()) { // Checks for double-booking using hash set
            cout << "Slot is already booked.\n";              // Reports collision
            return false;                                     // Signals failure
        }

        int apptId = nextAppointmentId++;                     // Generates a unique appointment ID
        Appointment appt(apptId, customerId, barberId, serviceId, dateTime, isVip); // Creates appointment object

        appointmentsById.emplace(apptId, appt);               // Stores appointment in map for O(1) access by ID
        activeAppointmentIds.push_back(apptId);               // Tracks active appointment ID in list for management
        bookedSlots.insert(slotKey);                          // Marks the barber-time slot as taken in O(1) avg time
        slotSet.erase(dateTime);                              // Removes from availability to enforce calendar booking
        upcomingQueue.push(apptId);                           // Adds appointment to priority queue for serving order

        cout << "Booked appointment ID: " << apptId << "\n";  // Confirms booking and provides reference ID
        return true;                                          // Signals success
    }

    bool cancelAppointment(int appointmentId) {
        auto it = appointmentsById.find(appointmentId);       // Locates appointment in hash map
        if (it == appointmentsById.end()) {                   // Checks if appointment exists
            cout << "Appointment not found.\n";               // Explains why cancellation failed
            return false;                                     // Signals failure
        }

        Appointment& appt = it->second;                       // Gets a mutable reference to update status
        if (appt.getStatus() != ApptStatus::Scheduled) {      // Prevents cancelling already-cancelled/completed
            cout << "Appointment is not in a schedulable state.\n"; // Explains state restriction
            return false;                                     // Signals failure
        }

        string slotKey = makeSlotKey(appt.getBarberId(), appt.getDateTime()); // Rebuilds slot key to release booking
        bookedSlots.erase(slotKey);                           // Removes the booking marker (slot becomes free)
        availableSlotsByBarber[appt.getBarberId()].insert(appt.getDateTime()); // Returns slot to availability set
        appt.setStatus(ApptStatus::Cancelled);                // Marks appointment as cancelled

        activeAppointmentIds.remove(appointmentId);           // Removes ID from list (O(n) but no shifting arrays)
        rebuildUpcomingQueue();                               // Rebuilds PQ to remove cancelled appointment reliably

        cout << "Cancelled appointment ID: " << appointmentId << "\n"; // Confirms cancellation
        return true;                                          // Signals success
    }

    bool rescheduleAppointment(int appointmentId, int newBarberId, const string& newDateTime) {
        auto it = appointmentsById.find(appointmentId);       // Finds appointment object by ID
        if (it == appointmentsById.end()) {                   // Validates appointment exists
            cout << "Appointment not found.\n";               // Reports error
            return false;                                     // Signals failure
        }
        if (!barberExists(newBarberId)) {                     // Validates new barber exists
            cout << "New barber not found.\n";                // Reports error
            return false;                                     // Signals failure
        }

        Appointment& appt = it->second;                       // Gets mutable appointment for updates
        if (appt.getStatus() != ApptStatus::Scheduled) {      // Only scheduled appointments can be rescheduled
            cout << "Only scheduled appointments can be rescheduled.\n"; // Explains rule
            return false;                                     // Signals failure
        }

        // Ensure new slot exists in availability set
        auto& newSlotSet = availableSlotsByBarber[newBarberId]; // Gets slot set for new barber
        if (newSlotSet.find(newDateTime) == newSlotSet.end()) {  // Checks requested slot is offered by staff
            cout << "That new time is not available for the selected barber.\n"; // Explains rule
            return false;                                       // Signals failure
        }

        string newSlotKey = makeSlotKey(newBarberId, newDateTime); // Build collision key for new slot
        if (bookedSlots.find(newSlotKey) != bookedSlots.end()) {   // Prevent double booking
            cout << "New slot is already booked.\n";               // Reports collision
            return false;                                          // Signals failure
        }

        // Release old slot back to availability
        string oldSlotKey = makeSlotKey(appt.getBarberId(), appt.getDateTime()); // Build old key
        bookedSlots.erase(oldSlotKey);                           // Remove old booked marker
        availableSlotsByBarber[appt.getBarberId()].insert(appt.getDateTime()); // Restore old slot to availability

        // Apply new slot
        appt.setBarberId(newBarberId);                           // Update barber assignment
        appt.setDateTime(newDateTime);                           // Update appointment time
        bookedSlots.insert(newSlotKey);                          // Mark new slot as booked
        newSlotSet.erase(newDateTime);                           // Remove from new barber availability

        rebuildUpcomingQueue();                                  // Rebuild queue to reflect new schedule ordering

        cout << "Rescheduled appointment ID: " << appointmentId << "\n"; // Confirmation output
        return true;                                              // Signals success
    }

    bool markCompleted(int appointmentId) {
        auto it = appointmentsById.find(appointmentId);           // Finds the appointment by ID
        if (it == appointmentsById.end()) {                       // Validates appointment exists
            cout << "Appointment not found.\n";                   // Reports error
            return false;                                         // Signals failure
        }

        Appointment& appt = it->second;                           // Gets mutable reference for state update
        if (appt.getStatus() != ApptStatus::Scheduled) {          // Only scheduled appointments can be completed
            cout << "Only scheduled appointments can be completed.\n"; // Explains rule
            return false;                                         // Signals failure
        }

        string slotKey = makeSlotKey(appt.getBarberId(), appt.getDateTime()); // Rebuilds slot key
        bookedSlots.erase(slotKey);                               // Removes booking marker (slot no longer occupied)
        appt.setStatus(ApptStatus::Completed);                    // Marks the appointment completed
        activeAppointmentIds.remove(appointmentId);               // Removes from active list
        rebuildUpcomingQueue();                                   // Updates priority ordering to remove completed appt

        cout << "Completed appointment ID: " << appointmentId << "\n"; // Confirmation
        return true;                                              // Signals success
    }

    void showNextAppointment() {
        while (!upcomingQueue.empty()) {                          // Loop until we find a valid scheduled appointment
            int apptId = upcomingQueue.top();                     // Reads the highest-priority appointment ID
            const Appointment& appt = appointmentsById.at(apptId); // Reads appointment details from map

            if (appt.getStatus() == ApptStatus::Scheduled) {      // Confirms appointment is still scheduled
                cout << "\nNext appointment:\n";                  // Prints heading
                printAppointment(appt);                           // Prints appointment details
                return;                                           // Exits after showing next
            }
            upcomingQueue.pop();                                  // Discards stale items that are not scheduled
        }
        cout << "No upcoming appointments.\n";                    // Reports that schedule is empty
    }

    void serveNextAppointment() {
        while (!upcomingQueue.empty()) {                          // Continue until we find a schedulable appointment
            int apptId = upcomingQueue.top();                     // Get next appointment ID
            const Appointment& appt = appointmentsById.at(apptId); // Inspect appointment state

            if (appt.getStatus() == ApptStatus::Scheduled) {      // Ensure it is scheduled
                upcomingQueue.pop();                              // Remove it from queue as it is being served now
                markCompleted(apptId);                            // Mark as completed to update structures correctly
                return;                                           // Exit after serving one appointment
            }
            upcomingQueue.pop();                                  // Discard stale cancelled/completed entries
        }
        cout << "No appointments to serve.\n";                    // Prints message when nothing is available
    }

    void printAllAppointments() const {
        cout << "\nAppointments (all):\n";                         // Heading for clarity
        if (appointmentsById.empty()) {                            // Checks if there are no appointments
            cout << "(none)\n";                                    // Reports empty dataset
            return;                                                // Exits early
        }
        for (const auto& pair : appointmentsById) {                // Iterates all appointments in map
            printAppointment(pair.second);                         // Prints each appointment’s details
        }
    }

    void printActiveSchedule() const {
        cout << "\nActive schedule (list order):\n";              // Heading for active schedule list
        if (activeAppointmentIds.empty()) {                        // Checks if there are no active appointment IDs
            cout << "(none)\n";                                    // Reports empty active schedule
            return;                                                // Exits early
        }
        for (int apptId : activeAppointmentIds) {                  // Iterates IDs in the linked list
            auto it = appointmentsById.find(apptId);               // Finds appointment record by ID
            if (it != appointmentsById.end()) {                    // Ensures it exists (safety check)
                printAppointment(it->second);                      // Prints the appointment details
            }
        }
    }

    void printScheduleByBarber(int barberId) const {
        cout << "\nSchedule for Barber " << barberId << ":\n";     // Prints barber-specific schedule heading
        bool any = false;                                          // Tracks whether we printed any appointments
        for (const auto& pair : appointmentsById) {                // Iterates all appointments to filter
            const Appointment& a = pair.second;                    // References appointment data
            if (a.getBarberId() == barberId) {                     // Filters by barber ID
                printAppointment(a);                               // Prints matching appointment
                any = true;                                        // Marks that we printed something
            }
        }
        if (!any) cout << "(none)\n";                              // Prints none if no match found
    }

private:
    void printAppointment(const Appointment& appt) const {
        cout << "ApptID: " << appt.getAppointmentId();             // Prints appointment identifier
        cout << " | CustomerID: " << appt.getCustomerId();         // Prints linked customer ID
        cout << " | BarberID: " << appt.getBarberId();             // Prints linked barber ID
        cout << " | ServiceID: " << appt.getServiceId();           // Prints linked service ID
        cout << " | Time: " << appt.getDateTime();                 // Prints appointment time string
        cout << " | VIP: " << (appt.getIsVip() ? "Yes" : "No");    // Prints VIP status
        cout << " | Status: " << statusToString(appt.getStatus()); // Prints status as text
        cout << "\n";                                              // Line break for readability
    }

    string statusToString(ApptStatus s) const {
        if (s == ApptStatus::Scheduled) return "Scheduled";        // Converts enum to user-friendly text
        if (s == ApptStatus::Cancelled) return "Cancelled";        // Converts enum to user-friendly text
        return "Completed";                                        // Converts enum to user-friendly text
    }
};

// -----------------------------
// Helpers for input
// -----------------------------

static void clearInputLine() {
    cin.ignore(numeric_limits<streamsize>::max(), '\n');           // Clears remaining input to avoid skipped reads
}

// -----------------------------
// Main menu
// -----------------------------

int main() {
    BookingSystem system;                                          // Creates the booking system object

    // Preload some demo data for easier testing (optional but useful for screenshots)
    int b1 = system.addBarber("Ali");                              // Adds barber Ali and stores returned barber ID
    int b2 = system.addBarber("Hassan");                           // Adds barber Hassan
    int s1 = system.addService("Haircut", 30, 15.00);              // Adds a haircut service
    int s2 = system.addService("Skin Fade", 45, 22.00);            // Adds a skin fade service
    system.addAvailableSlot(b1, "2026-06-01 10:00");               // Adds availability slot for Ali
    system.addAvailableSlot(b1, "2026-06-01 10:45");               // Adds another slot for Ali
    system.addAvailableSlot(b2, "2026-06-01 10:00");               // Adds availability slot for Hassan

    bool running = true;                                           // Controls the loop for the console menu

    while (running) {                                              // Repeats menu until user chooses exit
        cout << "\n--- Barber Shop Booking System ---\n";          // Displays menu title
        cout << "1) Add customer\n";                               // Menu option for customer creation
        cout << "2) List customers\n";                             // Menu option to view customers
        cout << "3) Add walk-in (customer ID)\n";                  // Menu option for walk-in queue
        cout << "4) Serve next walk-in\n";                         // Menu option to serve walk-in FIFO
        cout << "5) Add barber\n";                                 // Menu option to add barber
        cout << "6) List barbers\n";                               // Menu option to list barbers
        cout << "7) Add service\n";                                // Menu option to add service
        cout << "8) List services\n";                              // Menu option to list services
        cout << "9) Add available slot\n";                         // Menu option to define slot
        cout << "10) List available slots for barber\n";           // Menu option to view barber slots
        cout << "11) Book appointment\n";                          // Menu option to book from slots
        cout << "12) Cancel appointment\n";                        // Menu option to cancel
        cout << "13) Reschedule appointment\n";                    // Menu option to reschedule
        cout << "14) Mark appointment completed\n";                // Menu option to complete appointment
        cout << "15) Show next appointment\n";                     // Menu option to peek next
        cout << "16) Serve next appointment\n";                    // Menu option to serve next
        cout << "17) Print all appointments\n";                    // Menu option to print all
        cout << "18) Print active schedule\n";                     // Menu option for active list
        cout << "19) Print schedule by barber\n";                  // Menu option for filtering
        cout << "0) Exit\n";                                       // Menu option to exit program
        cout << "Choose: ";                                        // Prompt

        int choice;                                                // Stores user menu selection
        cin >> choice;                                             // Reads selection
        clearInputLine();                                          // Clears leftover newline

        switch (choice) {                                          // Branches based on user selection
        case 1: {
            string name, phone;                                    // Variables to hold new customer details
            cout << "Name: "; getline(cin, name);                  // Reads full line for name
            cout << "Phone: "; getline(cin, phone);                // Reads full line for phone
            int id = system.addCustomer(name, phone);              // Adds customer and captures new ID
            cout << "Customer added with ID: " << id << "\n";      // Prints confirmation
            break;                                                 // Exits case
        }
        case 2:
            system.listCustomers();                                // Calls method to display customers
            break;                                                 // Exits case
        case 3: {
            int customerId;                                        // Stores the ID for the walk-in customer
            cout << "Customer ID: "; cin >> customerId;            // Reads customer ID
            clearInputLine();                                      // Clears leftover newline
            system.addWalkIn(customerId);                          // Adds customer to walk-in queue
            cout << "Walk-in added.\n";                            // Confirmation
            break;                                                 // Exits case
        }
        case 4:
            system.serveWalkInIfAny();                             // Serves next walk-in in FIFO order
            break;                                                 // Exits case
        case 5: {
            string name;                                           // Stores barber name input
            cout << "Barber name: "; getline(cin, name);           // Reads barber name
            int id = system.addBarber(name);                       // Adds barber and gets new ID
            cout << "Barber added with ID: " << id << "\n";        // Prints confirmation
            break;                                                 // Exits case
        }
        case 6:
            system.listBarbers();                                  // Lists all barbers
            break;                                                 // Exits case
        case 7: {
            string name;                                           // Stores service name
            int dur;                                               // Stores service duration
            double price;                                          // Stores service price
            cout << "Service name: "; getline(cin, name);          // Reads service name
            cout << "Duration minutes: "; cin >> dur;              // Reads duration
            cout << "Price: "; cin >> price;                       // Reads price
            clearInputLine();                                      // Clears input buffer
            int id = system.addService(name, dur, price);          // Adds service and gets ID
            cout << "Service added with ID: " << id << "\n";       // Confirmation
            break;                                                 // Exits case
        }
        case 8:
            system.listServices();                                 // Lists all services
            break;                                                 // Exits case
        case 9: {
            int barberId;                                          // Stores barber ID
            string dateTime;                                       // Stores slot datetime string
            cout << "Barber ID: "; cin >> barberId;                // Reads barber ID
            clearInputLine();                                      // Clears newline
            cout << "Slot (YYYY-MM-DD HH:MM): "; getline(cin, dateTime); // Reads slot string
            bool ok = system.addAvailableSlot(barberId, dateTime); // Adds slot to availability set
            cout << (ok ? "Slot added.\n" : "Failed to add slot.\n"); // Prints result
            break;                                                 // Exits case
        }
        case 10: {
            int barberId;                                          // Stores barber ID
            cout << "Barber ID: "; cin >> barberId;                // Reads barber ID
            clearInputLine();                                      // Clears newline
            system.listAvailableSlots(barberId);                   // Displays barber slots
            break;                                                 // Exits case
        }
        case 11: {
            int customerId, barberId, serviceId;                   // Store booking inputs
            string dateTime;                                       // Store booking time
            int vipInt;                                            // Store VIP input as integer
            system.listCustomers();                                // Systems shows the list of the customers
            cout << "Customer ID: "; cin >> customerId;            // Reads customer ID
            system.listBarbers();                                   //System shows list of the Barbers
            cout << "Barber ID: "; cin >> barberId;                // Reads barber ID
            system.listServices();                                 //System shows the list of the services
            cout << "Service ID: "; cin >> serviceId;              // Reads service ID
            clearInputLine();                                      // Clears newline
            system.listAvailableSlots(barberId);                           //System shows list of available slots for the barber
            cout << "DateTime (YYYY-MM-DD HH:MM): "; getline(cin, dateTime); // Reads time string
            cout << "VIP? (1=yes, 0=no): "; cin >> vipInt;         // Reads VIP flag
            clearInputLine();                                      // Clears newline
            system.bookAppointment(customerId, barberId, serviceId, dateTime, vipInt == 1); // Attempt booking
            break;                                                 // Exits case
        }
        case 12: {
            int apptId;                                            // Stores appointment ID
            system.printAllAppointments();                         // system shows all the appointments that have been booked so far
            cout << "Appointment ID: "; cin >> apptId;             // Reads appointment ID
            clearInputLine();                                      // Clears newline
            system.cancelAppointment(apptId);                      // Cancels appointment
            break;                                                 // Exits case
        }
        case 13: {
            int apptId, newBarberId;                               // Stores reschedule IDs
            string newDateTime;                                    // Stores new datetime
            system.printAllAppointments();                         // Shows all the appointments that have been booked so far
            cout << "Appointment ID: "; cin >> apptId;             // Reads appointment ID
            system.listBarbers();                                  // System shows list of the Barbers
            cout << "New Barber ID: "; cin >> newBarberId;         // Reads new barber ID
            clearInputLine();                                      // Clears newline
            system.listAvailableSlots(newBarberId);
            cout << "New DateTime (YYYY-MM-DD HH:MM): "; getline(cin, newDateTime); // Reads new datetime
            system.rescheduleAppointment(apptId, newBarberId, newDateTime); // Attempts reschedule
            break;                                                 // Exits case
        }
        case 14: {
            int apptId;                                            // Stores appointment ID
            system.printAllAppointments();                         // Shows all the appointments
            cout << "Appointment ID: "; cin >> apptId;             // Reads appointment ID
            clearInputLine();                                      // Clears newline
            system.markCompleted(apptId);                          // Marks appointment completed
            break;                                                 // Exits case
        }
        case 15:
            system.showNextAppointment();                          // Shows next appointment without completing
            break;                                                 // Exits case
        case 16:
            system.serveNextAppointment();                         // Serves next appointment and marks completed
            break;                                                 // Exits case
        case 17:
            system.printAllAppointments();                         // Prints all appointments regardless of status
            break;                                                 // Exits case
        case 18:
            system.printActiveSchedule();                          // Prints currently active schedule list
            break;                                                 // Exits case
        case 19: {
            int barberId;                                          // Stores barber ID for filtering
            cout << "Barber ID: "; cin >> barberId;                // Reads barber ID
            clearInputLine();                                      // Clears newline
            system.printScheduleByBarber(barberId);                // Prints appointments filtered by barber
            break;                                                 // Exits case
        }
        case 0:
            running = false;                                       // Stops loop to exit program
            break;                                                 // Exits case
        default:
            cout << "Invalid choice.\n";                           // Handles invalid menu input
            break;                                                 // Exits default
        }
    }

    cout << "Goodbye!\n";                                          // Exit message
    return 0;                                                      // Returns success exit code
}
