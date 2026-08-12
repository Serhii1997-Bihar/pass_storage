# *Pass_Storage*

**PassStorage** is a console-based password and encrypted file storage manager written in **C++23**. The project provides secure storage for *text data*, *passwords*, *documents*, and *binary files* in a **PostgreSQL** database featuring Multi-Factor Authentication (MFA/2FA), OTP delivery via SMTP, and data export into password-protected `.7z` archives.

---

## 🛠 Technology Stack

* **Programming Language:** *C++23* (`std::format`, `std::ranges`, `std::optional`, Smart Pointers, RAII)
* **Database:** *PostgreSQL* (utilizing `libpqxx` library with `JSONB` and `BYTEA` data types)
* **Networking & Email:** *libcurl* (sending OTP verification codes via SMTP with TLS/SSL)
* **Formatting & JSON:** *{fmt}* library, *nlohmann/json*
* **Archiving & Security:** *7-Zip* utility (AES-256 / Header Encryption `-mhe=on`)

---

## 🏗 Architecture & Project Logic

The project follows a service-oriented architecture using standard smart pointers (`std::shared_ptr`) for safe resource management across database connections and transactions.

                               ┌──────────────────────┐
                               │      main.cpp        │
                               │ (CLI Orchestrator)   │
                               └──────────┬───────────┘
                                          │
                  ┌───────────────────────┼───────────────────────┐
                  ▼                       ▼                       ▼
          ┌──────────────┐         ┌──────────────┐      ┌──────────────────┐
          │ DB_Manager   │         │ Auth & OTP   │      │ Data & Save Mgr  │
          │ (PostgreSQL) │         │ (2FA & SMTP) │      │ (JSONB/BYTEA/7z) │
          └──────────────┘         └──────────────┘      └──────────────────┘


### Core Execution Workflow:

1. **Database Initialization**: Upon startup, `DB_Manager` loads configuration from the `.env` file, connects to PostgreSQL, and automatically runs DDL queries to create required tables (`users`, `data`, `file_storage`) if they do not exist.
2. **Authentication & Registration**:
    * **Registration**: The user enters their credentials along with *10 security questions and answers*, which are stored as a `JSONB` object. A default profile and data structure are initialized.
    * **Login (3-Stage Authentication)**:
        1. *Email* + *Password* verification.
        2. Generation and delivery of a 6-digit *OTP code* to the user's email via SMTP (`libcurl`).
        3. OTP verification followed by answering one randomly selected *security question* out of the 10 provided during registration.
3. **Data Operations**:
    * All *text entries* and *passwords* are stored inside PostgreSQL as a `JSONB` tree adhering to the `type/folder/key` hierarchy (`text`, `files`, `documents`).
    * Binary *files* are read into a memory buffer (`std::vector<char>`), assigned a unique `UUID v4`, and stored in the `file_storage` table (`BYTEA` column).
4. **Export & Secure Retrieval**:
    * When retrieving stored data or files, `Save_Manager` prepares a temporary working directory, exports the requested binary files or text from the database, packs them into a password-protected `.7z` archive, outputs the generated archive password to the console, and cleans up the temporary files.

---

## 📂 File Overview & Capabilities

| File                             | Description & Key Responsibilities                                                                                                                                                                                                  |
|:---------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **`db.cpp`**                     | **Database Manager (`DB_Manager`)**: Handles PostgreSQL connectivity via `libpqxx`. Loads environment variables and executes automatic schema migrations for `users`, `data`, and `file_storage` tables.                            |
| **`load_env.cpp` / `uuid.cpp`**  | **Helpers**: `load_env` parses the `.env` configuration file into an in-memory cache. `uuid` generates standard `UUID v4` strings using `std::mt19937` and `std::format`.                                                           |
| **`auth.cpp`**                   | **Authentication Service (`Auth`)**: Handles basic authentication (*Email* + *Password*) and advanced authentication. Randomly picks a security question from the user's `JSONB` questions array and validates the answer.          |
| **`otp.cpp`**                    | **OTP Manager (`Otp_Manager`)**: Generates 6-digit random verification codes and sends them to the user's email address over SMTP with TLS using `libcurl`.                                                                         |
| **`registration.cpp`**           | **Registration Service (`Registration`)**: Interactively collects user profile data, prompts for 10 security questions/answers, creates the user record, and initializes the default `JSONB` tree.                                  |
| **`data.cpp`**                   | **Data Manager (`Data_Manager`)**: Performs CRUD operations on the `JSONB` tree in PostgreSQL (`jsonb_set`, `#>`, `#-`). Handles reading binary files from disk into `file_storage` (`BYTEA`) and exporting them back out.          |
| **`saver.cpp`**                  | **Export Manager (`Save_Manager`)**: Dumps requested files/data into a temporary directory, compresses them into an encrypted `.7z` archive with hidden headers (`-mhe=on`) via **7-Zip**, and returns the random archive password. |
| **`user.cpp`**                   | **User Profile Manager (`User_Manager`)**: Provides functions to retrieve and update user account details (such as *email* or *password*) in the database.                                                                          |
| **`main.cpp`**                   | **Entry Point (CLI Orchestrator)**: Ties all services together into an interactive command-line interface, managing the authentication loop and user menu routing.                                                                  |
| **`hashing.cpp / cryption.cpp`** | **Crypto**: These files are the main helpers to hash and crypt data to provide security of user.                                                                                                                                    |

---

## 🗄 Database Schema (PostgreSQL)

The schema is created automatically on the first application run:

* **`users`**: Stores user credentials, phone numbers, active OTP codes, and a `JSONB` object containing the 10 security questions and answers.
* **`data`**: Holds the user-bound `JSONB` tree. Mandatory root categories include `text`, `files`, and `documents`.
* **`file_storage`**: Stores uploaded binary files using `UUID` as the primary key and `BYTEA` as the binary content container.

---