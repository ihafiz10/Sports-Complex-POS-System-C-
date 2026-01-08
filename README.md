# Sports Complex POS System (C++)

This project is a console-based Point of Sale (POS) system developed in C++ for managing operations in a sports complex.

The system supports facility bookings, membership registration, equipment rental, product sales, locker rental, payments, refunds, and sales reporting, simulating real-world business workflows in a sports centre environment.

This project was built as part of an academic assignment and enhanced to demonstrate basic object-oriented programming concepts, file handling, validation, and introductory system design principles.

## Features
- Membership registration with automatic expiry calculation
- Hourly court booking with time clash detection
- Per-entry access (gym, swimming pool, fitness studio)
- Equipment rental with deposit handling
- Merchandise & snack sales with inventory tracking
- Shopping cart and checkout system
- Promo code and member discount support
- SST (6%) calculation
- Booking cancellation and refund with stock restoration
- Daily and monthly sales reports
- Admin panel for inventory, members, bookings, and reports
- Persistent data storage using text files

## Technologies Used
- Language: C++
- Concepts: Object-Oriented Programming (basic data modeling), File Handling
- Data Storage: Text file persistence
- Libraries: STL (vector, map, algorithm, iostream, fstream)

## How to Run
- Compile using a C++ compiler (tested with Visual Studio 2022)
- Run the compiled executable

> Note: All data (members, bookings, inventory) are saved locally using text files.

## Future Improvements
- Migrate data storage to a database (MySQL)
- Refactor booking records into structured objects
- Add user authentication and role-based access
- Convert system into a web-based application (Java Spring Boot)

## Project Structure
- src/main.cpp — main POS system (v1 monolithic implementation)
- practice/ — small OOP practice and learning experiments


