# Dbin: A secure, distributed, command-line-based file-sharing tool, made using C, for Linux.
---

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/<your_username>/<your_repo>) <!-- Replace with your actual build badge if you set one up -->
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Dbin provides a lightweight, secure, and efficient way for users on a local Linux network to share files directly or store them on a central server. Designed entirely in C, it features distinct user roles, secure IP-based access control, and robust file transfer capabilities using a hybrid UDP/TCP approach for optimal performance and large file support. The entire tool is command-line operable.

* The Dbin system has three actors: A Super User, up to 10 Normal Users and a Central Repository.
* The Super User is the Admin. If a system is assigned as a Super User, it can control the sharing of files in the network. A Super User can send files to all other Normal Users as well as the Central Repository for storage. The Super User initialises the system as well as terminates it.
* The Normal User(s) are systems without root priviledges. They can share files with other Normal Users, Super User as well as the Central Repository.
* The Central Repository is the System which acts like a centralised storage unit. It utilises the capabilities of the SQLite Database for storing the file information. It can receive files from both the users (Normal and Super) as well as send files back to their respective owners. It allows the Super User to look at all the files which are currently stored in it. However, a Normal User can only see the files deleted by them. Also, the Super User can clear the contents of the Central Repository (files and metadata), but the Normal User can't. However, if any User (both Super and Normal) wants to retrieve their file from the Central Repository, then they can only get their files back and not others'. This feature ensures data integrity.  

Abbreviations: 
* **SU or su:** Refers to Super User
* **NU or nu:** Refers to Normal User
* **CR or cr:** Refers to Central Repository

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
* **Secure within LAN:** Communication restricted to authorised IPs defined by the Super User.
* **Flexible File Sharing:** SU -> NU, NU -> SU, NU -> NU, Any -> CR, CR -> Any (The User which raised the query).
* **Centralised Storage:** Temporary storage on CR with user-specific retrieval.
* **Administrative Controls:** SU can view all CR files (`fsee`), clear the CR database (`cleardb`), and shut down the system (`kall`).
* **Large File Support:** Reliable TCP streaming for files exceeding UDP limits.

### Commands

#### On the Super User terminal (`./su`)

* `fnu <nu_ip> <filepath>`: Send a file to a Normal User.
* `fdel <cr_ip> <filepath>`: Send a file to the Central Repository for storage.
* `fsee <cr_ip>`: View all files currently stored in the Central Repository.
* `fback <cr_ip> <filename>`: Retrieve your own previously stored file from the CR.
* `cleardb <cr_ip>`: Clear all file records from the Central Repository database.
* `kall`: Send a termination signal to all NU(s) and the CR, then exit.

#### On the Normal User terminal (`./nu`)

* `fsu <su_ipaddress> <filepath>`: Send a file to the Super User.
* `fnu <nu_ipaddress> <filepath>`: Send a file to another Normal User.
* `fdel <cr_ipaddress> <filepath>`: Send a file to the Central Repository for storage.
* `seemyfiles <cr_ipaddress>`: View only your files currently stored in the Central Repository.
* `fback <cr_ipaddress> <filename>`: Retrieve your own previously stored file from the CR.
* `exit`: Exit the Normal User client program.

*(Note: Replace `<..._ipaddress>` and `<filename/filepath>` with actual values.)*

---
## File Structure 📂
There are three directories - Super_User, Normal_User and Central_Repository. Each directory contains: 
* **.c file: ** Main C program
* **Makefile: ** For building the '.c' file
* **set_firewall script file:** For configuring firewall settings to allow ports for communication.

Every directory is independent of the other. If you're running the Super_User program on this machine, you need not download and run the other two programs, same for Normal_User and Central_Repository.
---
## Dependencies 📦

* **GCC Compiler & Build Tools:** (`build-essential` on Debian/Ubuntu)
* **Make:** (Included in `build-essential`)
* **SQLite3 Development Library:** (`libsqlite3-dev` on Debian/Ubuntu) - Required *only* for compiling the Central Repository.

---

## Installation and Building ⚙️

1.  **Clone the Repository or Download the contents of the corresponding file.**

2.  **Install Dependencies:** Ensure you have GCC, Make, and (for the CR machine) the SQLite3 development library installed using your distribution's package manager.

3.  **Compile Programs:** For each designated system, navigate into the respective directory and use `make`. If any changes are made in any files, run `make clean` first and then run `make`:
   
    For Central_Repository
    ```bash
    cd Central_Repository
    make
    ```
    For Normal_User(s)
    ```bash
    cd Normal_User
    make
    ```
    For Super_User
    ```bash
    cd Super_User
    make
    ```
    This will create the executables: `cr` for Central_Repository, `nu` for Normal_User, and `su` for Super_User in their respective directories.

---

## Running the System ▶️

**Important:** All machines must be on the same Local Area Network (LAN).

1.  **Configure Firewalls:** Before the first run on each machine, execute the corresponding firewall setup script with `sudo`. You will be prompted for confirmation.
    * On the **Central Repository** machine: `sudo bash Central_Repository/setup_firewall_cr.sh`
    * On the **Normal User** machine(s): `sudo bash Normal_User/setup_firewall_nu.sh`
    * On the **Super User** machine: `sudo bash Super_User/setup_firewall_su.sh`

2.  **Start Programs (Order Matters!):**
    * **First, on the system designated as Central Repository, start the Central Repository:**
        ```bash
        ./cr
        ```
    * **Next, on the system(s) designated as Normal User(s), start all Normal User clients:**
        ```bash
        ./nu
        ```
    * **Finally, on the system designated as Super User, start the Super User:**
        ```bash
        ./su
        ```
    * Follow the prompts to enter the number of Normal Users (Max 10) and the correct **network IP addresses** for all machines (NUs, CR, and the SU machine itself).

3.  **Use the System:** Once the Super User provides the IPs, the system is initialised, and you can use the commands listed in the "Features & Usage Guide" section.

---

## License 📄

This project is licensed under the **MIT License**. See the `LICENSE` file for details.
---
