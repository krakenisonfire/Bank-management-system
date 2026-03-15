# **DAIICT Bank Management System (Raylib C GUI)**

A complete multi-user banking system built in **C** with a sleek graphical interface using the **Raylib** library. The system is modular, ensuring clean separation of functionality across different executables (Login, User, Staff, Admin).

---

##  **Project Architecture**
This project consists of **four main modules**, each compiled into its own executable:

1. **`login.exe`** – Entry point of the system; handles authentication for all user types.
2. **`userinterface.exe`** – Customer banking dashboard.
3. **`staff_menu.exe`** – Staff operations and account handling.
4. **`admin_interface.exe`** – Administrative controls and analytics.

Upon successful login, `login.exe` launches the respective interface using:
```
system("userinterface.exe <username>");
```

---

## 🛠️ **Installation (MSYS2 – Windows)**

### **1️⃣ Open MSYS2 and Navigate to Your Project Directory**
Before running any compilation command, open **MSYS2 MinGW64** and move to your project folder:
```
cd /e/project_folder_name
```
*(Example: If files are in `E:/BankSystem`, then run `cd /e/BankSystem`)*

---

## 🧱 **2️⃣ Compilation**
Compile each module separately with Raylib linked.

### **Login Module:**
```
gcc login.c -o login.exe -lraylib -lgdi32 -lwinmm 
```

### **User Interface:**
```
gcc userinterface.c -o userinterface.exe -lraylib -lgdi32 -lwinmm 
```

### **Staff Interface:**
```
gcc staffinterface.c -o staff_menu.exe -lraylib -lgdi32 -lwinmm 
```

### **Admin Interface:**
```
gcc admin_interface.c -o admin_interface.exe -lraylib -lgdi32 -lwinmm 
```

---

## ▶️ **3️⃣ Running the System**
Make sure all `.exe` files and all required data files are in the same directory:
- `users.txt`
- `staff.txt`
- `admin.txt`
- Transaction/loan/log files (as applicable)

### Start the system:
```
./login.exe
```
You will be greeted with the main menu for User / Staff / Admin.

---

## 📂 **Data Flow Overview**
- Each executable works independently.
- Data is exchanged via text files.
- Login verifies credentials and launches the correct interface.
- User/Staff/Admin interfaces read & update respective data files.

This architecture keeps the project modular, readable, and easily maintainable.

---




