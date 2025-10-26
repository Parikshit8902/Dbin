# DBIN: Distributed File Sharing System for Linux

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/<your_username>/<your_repo>) <!-- Replace with your actual build badge if you set one up -->
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

DBIN provides a lightweight, secure, and efficient way for users on a local Linux network to share files directly or store them temporarily on a central server. Designed entirely in C, it features distinct user roles, secure IP-based access control, and robust file transfer capabilities using a hybrid UDP/TCP approach for optimal performance and large file support.

---

## Tools & Technology Used 🛠️

* **Language:** C
* **Networking:**
    * UDP Sockets (for control messages, commands, handshakes)
    * TCP Sockets (for reliable, large file data transfer)
    * POSIX Sockets API
* **Concurrency:** POSIX Threads (pthreads)
* **Database:** SQLite3 (on Central Repository)
* **Build System:** Make
* **Platform:** Linux

---

## Features & Usage Guide ✨⌨️

### Key Features

* **Distinct User Roles:** Super User (admin), Normal User (client), Central Repository (server).
* **Secure within LAN:** Communication restricted to authorized IPs defined by the Super User.
* **Flexible File Sharing:** SU -> NU, NU -> SU, NU -> NU, Any -> CR.
* **Centralized Storage:** Temporary storage on CR with user-specific retrieval.
* **Administrative Controls:** SU can view all CR files (`fsee`), clear the CR database (`cleardb`), and shut down the system (`kall`).
* **Large File Support:** Reliable TCP streaming for files exceeding UDP limits.

### Commands

#### Super User (`su_controller`)

* `fnu <nu_ip> <filepath>`: Send a file to a Normal User.
* `fdel <cr_ip> <filepath>`: Send a file to the Central Repository for storage.
* `fsee <cr_ip>`: View all files currently stored in the Central Repository.
* `fback <cr_ip> <filename>`: Retrieve your own previously stored file from the CR.
* `cleardb <cr_ip>`: Clear all file records from the Central Repository database.
* `kall`: Send a termination signal to all NUs and the CR, then exit.

#### Normal User (`nu_client`)

* `fsu <su_ip> <filepath>`: Send a file to the Super User.
* `fnu <nu_ip> <filepath>`: Send a file to another Normal User.
* `fdel <cr_ip> <filepath>`: Send a file to the Central Repository for storage.
* `seemyfiles <cr_ip>`: View only your files currently stored in the Central Repository.
* `fback <cr_ip> <filename>`: Retrieve your own previously stored file from the CR.
* `exit`: Exit the Normal User client program.

*(Note: Replace `<..._ip>` and `<filename/filepath>` with actual values.)*

---

## Dependencies 📦

* **GCC Compiler & Build Tools:** (`build-essential` on Debian/Ubuntu)
* **Make:** (Included in `build-essential`)
* **SQLite3 Development Library:** (`libsqlite3-dev` on Debian/Ubuntu) - Required *only* for compiling the Central Repository.

---

## Installation and Building ⚙️

1.  **Clone the Repository:**
    ```bash
    git clone <your-repository-url>
    cd <repository-directory>
    ```

2.  **Install Dependencies:** Ensure you have GCC, Make, and (for the CR machine) the SQLite3 development library installed using your distribution's package manager.

3.  **Compile Programs:** Navigate into each directory and use `make`:
    ```bash
    cd Central_Repository && make && cd ..
    cd Normal_User && make && cd ..
    cd Super_User && make && cd ..
    ```
    This will create the executables: `cr_server`, `nu_client`, and `su_controller` in their respective directories.

---

## Running the System ▶️

**Important:** All machines must be on the same Local Area Network (LAN).

1.  **Configure Firewalls:** Before the first run on each machine, execute the corresponding firewall setup script with `sudo`. You will be prompted for confirmation.
    * On the **Central Repository** machine: `sudo bash Central_Repository/setup_firewall_cr.sh`
    * On the **Normal User** machine(s): `sudo bash Normal_User/setup_firewall_nu.sh`
    * On the **Super User** machine: `sudo bash Super_User/setup_firewall_su.sh`

2.  **Start Programs (Order Matters!):**
    * **First, start the Central Repository:**
        ```bash
        ./Central_Repository/cr_server
        ```
    * **Next, start all Normal User clients:**
        ```bash
        ./Normal_User/nu_client
        ```
        *(They will wait for the IP table.)*
    * **Finally, start the Super User:**
        ```bash
        ./Super_User/su_controller
        ```
        * Follow the prompts to enter the number of Normal Users and the correct **network IP addresses** for all machines (NUs, CR, and the SU machine itself).

3.  **Use the System:** Once the Super User provides the IPs, the system is initialized, and you can use the commands listed in the "Features & Usage Guide" section.

---

## License 📄

This project is licensed under the **MIT License**. See the `LICENSE` file for details.
---

## File Structure 📂
