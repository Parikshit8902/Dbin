# DBIN: Distributed File Sharing System for Linux

**This is a secure, distributed file sharing tool, designed using C, for Linux.**

DBIN provides a lightweight and efficient way for users on a local Linux network to share files directly or store them temporarily on a central server. It features distinct user roles, secure IP-based access control, and robust file transfer capabilities using both UDP and TCP.

---

## Features ✨

* **Distinct User Roles:**
    * **Super User (SU):** Initializes the network, manages users, and has administrative access to the Central Repository.
    * **Normal User (NU):** Can share files with other users (SU and NU) and utilize the Central Repository for storage/retrieval.
    * **Central Repository (CR):** Acts as a central server for temporary file storage and retrieval, with access controls based on the sender's IP.
* **Secure within LAN:** Communication is restricted to authorized IP addresses provided during initialization. The Central Repository validates incoming requests against this list.
* **Flexible File Sharing:**
    * SU can send files to any NU.
    * NU can send files to the SU or any other NU.
    * Any user can send files to the CR for temporary storage (`fdel`).
* **Centralized Storage & Retrieval:**
    * Users can retrieve files they previously sent to the CR (`fback`). Files are deleted from the CR after retrieval.
    * Normal Users can view a list of only *their* files currently stored on the CR (`seemyfiles`).
* **Administrative Controls:**
    * Super User can view a list of *all* files stored on the CR (`fsee`).
    * Super User can clear the entire database of the CR (`cleardb`).
* **Large File Support:** Utilizes TCP for reliable, streaming transfer of large files (like images, videos, archives) between users and the repository. UDP is used for smaller control messages and commands.
* **System Termination:** Super User can gracefully shut down all connected clients and the central server (`kall`).

---

## Tools & Technology 🛠️

* **Language:** C
* **Networking:**
    * UDP Sockets (for control messages, commands, and handshakes)
    * TCP Sockets (for reliable, large file data transfer)
    * POSIX Sockets API
* **Concurrency:** POSIX Threads (pthreads) for handling asynchronous operations (listening, file transfers).
* **Database:** SQLite3 (for storing file metadata on the Central Repository).
* **Build System:** Makefiles
* **Platform:** Linux

---

## Getting Started 🚀

Follow these steps to set up and run the DBIN system on your Linux machines.

### Prerequisites

* **GCC Compiler:** Required to compile the C code (`sudo apt install build-essential` on Debian/Ubuntu).
* **Make:** Required to use the Makefiles (`sudo apt install build-essential`).
* **SQLite3 Development Library:** Required *only* on the machine running the Central Repository (`sudo apt install libsqlite3-dev`).
* **Local Network:** All participating machines must be on the same local network.

### Setup

1.  **Clone the Repository:**
    ```bash
    git clone <your-repository-url>
    cd <repository-directory>
    ```

2.  **Configure Firewalls:** Before running the programs, you must open the necessary UDP and TCP ports on each machine's firewall. Helper scripts are provided:
    * On the **Normal User** machine: `sudo bash Normal_User/setup_firewall_nu.sh`
    * On the **Super User** machine: `sudo bash Super_User/setup_firewall_su.sh`
    * On the **Central Repository** machine: `sudo bash Central_Repository/setup_firewall_cr.sh`
    *(These scripts will prompt for confirmation before applying rules.)*

3.  **Compile the Programs:** Navigate into each user directory and run `make`.
    * `cd Normal_User && make`
    * `cd ../Super_User && make`
    * `cd ../Central_Repository && make`

### Running the System

You must run the programs in the correct order:

1.  **Start Central Repository:** On the CR machine:
    ```bash
    ./Central_Repository/cr_server
    ```
2.  **Start Normal Users:** On each NU machine:
    ```bash
    ./Normal_User/nu_client
    ```
    *(They will wait for the IP table.)*
3.  **Start Super User:** On the SU machine:
    ```bash
    ./Super_User/su_controller
    ```
    * Follow the prompts to enter the number of Normal Users and the correct network IP addresses for all machines (including the SU machine itself).
    * Once the SU provides the IPs, the system will initialize, and all clients will become active.

---

## Usage Guide ⌨️

### Super User Commands (`su_controller`)

* `fnu <nu_ip> <filepath>`: Send a file to a Normal User.
* `fdel <cr_ip> <filepath>`: Send a file to the Central Repository for storage.
* `fsee <cr_ip>`: View all files currently stored in the Central Repository.
* `fback <cr_ip> <filename>`: Retrieve your own previously stored file from the CR.
* `cleardb <cr_ip>`: Clear all file records from the Central Repository database.
* `kall`: Send a termination signal to all NUs and the CR, then exit.

### Normal User Commands (`nu_client`)

* `fsu <su_ip> <filepath>`: Send a file to the Super User.
* `fnu <nu_ip> <filepath>`: Send a file to another Normal User.
* `fdel <cr_ip> <filepath>`: Send a file to the Central Repository for storage.
* `seemyfiles <cr_ip>`: View only your files currently stored in the Central Repository.
* `fback <cr_ip> <filename>`: Retrieve your own previously stored file from the CR.
* `exit`: Exit the Normal User client program.

*(Note: Replace `<..._ip>` and `<filename/filepath>` with actual values.)*

---

## Limitations

* **LAN Only:** The system currently works only within a single Local Area Network. Communication across different networks (WAN/Internet) requires additional configuration like port forwarding and potentially NAT traversal techniques.
* **Single TCP Port for Uploads:** The current implementation uses a single, well-known TCP port (`9000`) for receiving file uploads. While transfers are handled in separate threads, simultaneous *attempts* to upload to the same recipient might encounter contention for this port.

---

## License 📄

This project is licensed under the **MIT License**. See the `LICENSE` file for details.
