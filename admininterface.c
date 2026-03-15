/***********************************************************************************
 * admin_gui.c
 * -----------------------------------------------------------------------------
 * Raylib GUI for Admin Module (Symmetric Design)
 *
 * Features:
 * - Banking Admin: View All, Delete, Block/Unblock, Search Account, View Changelog,
 * Create Staff Account, View Loan Status
 * - Care Admin: View All Ratings, View All Complaints
 * - V2 Change: Added Loan Approval system.
 * - V3 Change: Block/Unblock card now syncs with users.txt to fix login bug.
 * - V4 Change (User Request): Loan approval screen now shows status instead of buttons for processed loans.
 * - V5 Change (User Request): Added new "View Loan Status" screen to read from temp_loan.txt.
 * - V6 Change (User Request): Added backup logic to copy temp_loan.txt to loan_backup.txt before updates.
 * - V7 Fix: Use a distinct temp file (loan.tmp) when updating loan statuses to avoid filename collisions.
 *
 * *** V8 FIX (CURRENT): Solved file lock conflict in APPROVE_LOANS. ***
 * - Reads all loans into an in-memory array *before* drawing.
 * - Closes file, removing the lock.
 * - Draws UI from the array, allowing Logic_UpdateLoanStatus to run successfully.
 * - Added a 'needsRefresh' system to reload data immediately after an update.
 *
 * *** V9 CHANGE (USER REQUEST): Consolidated Care Analytics features. ***
 * - Combined 3 complaint features into VIEW_ALL_COMPLAINTS.
 * - This new screen reads from complaints.csv and shows complaint status.
 * - Reworked VIEW_RATING_STATS to be VIEW_ALL_RATINGS.
 * - This screen now reads from ratings.csv and displays all ratings symmetrically.
 *
 * Compile (Windows Example):
 * gcc admin_gui.c -o admin_gui.exe -lraylib -lgdi32 -lwinmm -Wl,-subsystem,windows
 ***********************************************************************************/

#include "raylib.h"         // Core Raylib library for GUI, drawing, and input
#include <stdio.h>          // Standard input/output for file operations (fopen, printf, etc.)
#include <stdlib.h>         // Standard library for system(), atoi(), atof()
#include <string.h>         // String manipulation functions (strcpy, strcmp, etc.)
#include <time.h>           // For generating timestamps (time, localtime, strftime)
#include <math.h>           // For loan logic (not currently used, but included)

//----------------------------------------------------------------------------------
// Defines and Types
//----------------------------------------------------------------------------------
#define SCREEN_WIDTH 1620       // Define the width of the application window
#define SCREEN_HEIGHT 920       // Define the height of the application window
#define MAX_INPUT_CHARS 50      // Maximum characters for input fields
#define MAX_ACCOUNTS 100        // V3 FIX: Max accounts for users.txt in-memory array
#define MAX_ADMIN_LOANS 100     // V8 FIX: Max loans to load into memory for admin approval screen

// File Paths
#define FILE_NAME         "accounts.csv"      // Main customer accounts database (created by staff)
#define TRANSACTION_FILE  "transactions.csv"  // Log of all transactions
#define CHANGELOG_FILE    "changelog.csv"     // Log of staff actions (e.g., account creation)
#define COMPLAINT_FILE    "complaints.csv"    // Customer complaints log
#define RATING_FILE       "ratings.csv"       // Customer ratings log
#define LOAN_FILE         "temp_loan.txt"     // Main file for pending and processed loans
#define LOAN_BACKUP_FILE  "loan_backup.txt"   // Backup of LOAN_FILE, created before updates
#define LOAN_TEMP_FILE    "loan.tmp"          // Temporary file used during loan status updates
#define USER_FILE         "users.txt"         // File for customer login credentials (synced with accounts.csv)
#define STAFF_FILE        "staff.txt"         // File for staff login credentials

// Enum to manage which screen is currently displayed
typedef enum {
    MENU,                   // The main navigation menu
    VIEW_ALL_ACCOUNTS,      // Display all customer accounts from accounts.csv
    DELETE_ACCOUNT,         // Screen to delete a customer account
    BLOCK_UNBLOCK_CARD,     // Screen to freeze/unfreeze a customer's account/card
    SEARCH_ACCOUNT,         // Screen to find and display a single customer's details
    VIEW_CHANGELOG,         // Display the staff action log (changelog.csv)
    VIEW_ALL_RATINGS,       // Display all customer ratings (ratings.csv)
    VIEW_ALL_COMPLAINTS,    // Display all customer complaints (complaints.csv)
    APPROVE_LOANS,          // Screen to approve or reject pending loans (temp_loan.txt)
    CREATE_STAFF,           // Screen to create a new staff login (staff.txt)
    VIEW_LOAN_STATUS,       // Read-only screen to view all loans (temp_loan.txt)
    // New Staff Operations
    VIEW_ALL_STAFF,         // Display all staff accounts (staff.txt)
    SEARCH_STAFF            // Screen to check if a staff ID exists
} AdminScreen;

// --- Structs copied from userinterface.c for loan/account logic ---
// Struct for users.txt (customer login file)
typedef struct {
    char username[MAX_INPUT_CHARS + 1]; // This is the Account Number
    char password[MAX_INPUT_CHARS + 1]; // This is the PIN
    double balance;                     // Current balance
    bool isCardBlocked;                 // Block status (1 for blocked, 0 for active)
} Account;

// Struct for accounts.csv (main customer database)
typedef struct {
    int accountNumber;      // Unique account number
    char name[100];         // Customer's full name
    char pin[10];           // Customer's PIN
    double balance;         // Current balance
    int cardBlocked;        // Block status (1 for blocked, 0 for active)
    char cardNumber[24];    // Customer's card number
    char createdBy[50];     // Staff ID of the creator
} StaffAccount;

// Struct for temp_loan.txt
struct Loan {
    int acc_no, loan_id, tenure, type; // Loan details
    char status[20];                  // "Pending", "Approved", or "Rejected"
    float amount, rate, emi;          // Loan financial details
};
// --- End Structs ---

// UI State
static AdminScreen currentScreen = MENU;  // Tracks the current active screen
static char statusMessage[256] = { 0 };   // Stores feedback messages (e.g., "Success: ...")
static Vector2 scrollPosition = { 0, 0 }; // Stores the Y-scroll offset for list views
static bool needsRefresh = false;         // Flag for loan screen to reload data after an update
static StaffAccount searchResultAccount = { 0 }; // Holds the found account data for the SEARCH_ACCOUNT screen

// V8 FIX: In-memory array for loans
static struct Loan adminLoanList[MAX_ADMIN_LOANS]; // Array to hold loans read from file
static int adminLoanCount = 0;                     // Number of loans currently in the array

// Input handling (MODIFIED to include isPassword)
typedef struct {
    Rectangle bounds;                   // Position and size of the text box
    char text[MAX_INPUT_CHARS + 1];     // The text content
    int charCount;                      // Current number of characters
    bool active;                        // Is the box currently selected?
    bool isPassword;                    // Should the text be masked (e.g., "********")?
} TextBox;

static TextBox inputAccountBox;     // Textbox for entering customer account numbers
static TextBox inputStaffIdBox;     // Textbox for new staff ID
static TextBox inputStaffPassBox;   // Textbox for new staff password
static TextBox inputStaffSearchBox; // Textbox for searching staff ID

//----------------------------------------------------------------------------------
// Forward Declarations for Logic Functions
//----------------------------------------------------------------------------------
void Logic_DeleteAccount(int accNo);                                     // Deletes an account from accounts.csv
void Logic_BlockUnblockCard(int accNo);                                  // Toggles block status in accounts.csv and users.txt
void Logic_UpdateLoanStatus(int loanId, const char* newStatus);          // Updates a loan's status in temp_loan.txt
bool Logic_AccountExists(int accNo);                                     // Checks if an account exists in accounts.csv
bool Logic_UpdateUsersFile(const char *accNoStr, bool isBlocked);        // Syncs block status to users.txt
void Logic_CreateStaffAccount(const char *staffId, const char *password);// Adds a new staff login to staff.txt
bool Logic_StaffExists(const char *staffId);                             // Checks if a staff ID exists in staff.txt

static StaffAccount getStaffAccount(int accNo);                          // Reads a single account's details from accounts.csv
static void updateStaffAccount(StaffAccount acc);                        // Rewrites a single account's details in accounts.csv
static void getCurrentTimestamp(char *buffer, size_t size);              // Gets the current date/time as a string
static void logTransaction(int accNo, const char *type, double amount, double balance); // Appends a transaction to transactions.csv
static void Logic_BackupLoanFile(void);                                  // Copies temp_loan.txt to loan_backup.txt
static void Logic_LoadAdminLoans(void);                                  // V8 FIX: Loads all loans from temp_loan.txt into memory

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void InitTextBox(TextBox *box, Rectangle bounds, bool isPassword); // Initializes/resets a text box
static void UpdateTextBox(TextBox *box);                                  // Handles keyboard input for a text box
static void DrawTextBox(TextBox *box, const char *placeholder);           // Draws a text box (and placeholder)
static bool DrawButton(Rectangle bounds, const char *text);               // Draws a button and returns true if clicked
static void DrawBackButton(void);                                         // Draws the standard "Back to Menu" button

//----------------------------------------------------------------------------------
// Main Entry Point
//----------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    if (argc<3) return 0;
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Admin Dashboard"); // Create the window
    SetTargetFPS(60); // Set target FPS

    // MODIFIED: InitTextBox now requires the isPassword boolean
    float boxWidth = 400; // Standard width for input boxes
    float startX = (SCREEN_WIDTH - boxWidth) / 2; // Center-aligned X position

    // Initialize all text boxes
    InitTextBox(&inputAccountBox, (Rectangle){ startX, 300, boxWidth, 50 }, false); // For customer account numbers (not password)

    // NEW STAFF TEXTBOXES initialization
    InitTextBox(&inputStaffIdBox, (Rectangle){ startX, 300, boxWidth, 50 }, false); // For new staff ID (not password)
    InitTextBox(&inputStaffPassBox, (Rectangle){ startX, 300 + 70, boxWidth, 50 }, true); // For new staff password (is password)
    InitTextBox(&inputStaffSearchBox, (Rectangle){ startX, 300, boxWidth, 50 }, false); // For searching staff ID (not password)

    while (!WindowShouldClose()) // Main application loop
    {
        // Update
        Vector2 mouse = GetMousePosition(); // Get mouse position every frame
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { // Check for a click
            // Deactivate all text boxes first
            inputAccountBox.active = false;
            inputStaffIdBox.active = false;
            inputStaffPassBox.active = false;
            inputStaffSearchBox.active = false; // NEW

            // Activate specific box based on screen and click collision
            if (currentScreen == DELETE_ACCOUNT || currentScreen == BLOCK_UNBLOCK_CARD || currentScreen == SEARCH_ACCOUNT) {
                if (CheckCollisionPointRec(mouse, inputAccountBox.bounds)) inputAccountBox.active = true;
            } else if (currentScreen == CREATE_STAFF) {
                if (CheckCollisionPointRec(mouse, inputStaffIdBox.bounds)) inputStaffIdBox.active = true;
                if (CheckCollisionPointRec(mouse, inputStaffPassBox.bounds)) inputStaffPassBox.active = true;
            } else if (currentScreen == SEARCH_STAFF) { // NEW
                if (CheckCollisionPointRec(mouse, inputStaffSearchBox.bounds)) inputStaffSearchBox.active = true;
            }
        }
        // Update the content of any active text box
        UpdateTextBox(&inputAccountBox);
        UpdateTextBox(&inputStaffIdBox);
        UpdateTextBox(&inputStaffPassBox);
        UpdateTextBox(&inputStaffSearchBox); // NEW

        float wheel = GetMouseWheelMove(); // Check for mouse wheel movement
        if (wheel != 0) scrollPosition.y += wheel * 20; // Adjust scroll position

        // V8 FIX: This logic reloads data if a change was made
        if (needsRefresh) {
            if (currentScreen == APPROVE_LOANS) {
                Logic_LoadAdminLoans(); // Reload the loan data from file into memory
            }
            // Add other screen refreshes here if needed
            needsRefresh = false; // Reset the flag after refreshing
        }

        // Draw
        BeginDrawing(); // Start drawing phase
        ClearBackground((Color){ 245, 245, 245, 255 }); // Set light gray background

        // Draw header bar
        DrawRectangle(0, 0, SCREEN_WIDTH, 80, (Color){ 60, 90, 150, 255 }); // Dark blue bar
        DrawText("Admin Dashboard", 30, 20, 40, WHITE); // Title

        // Draw logout button
        if (DrawButton((Rectangle){ SCREEN_WIDTH - 180, 20, 150, 40 }, "Logout")) {
            system("start login.exe"); // Launch the login program
            CloseWindow();             // Close this admin program
            return 0;                  // Exit
        }

        switch (currentScreen) // Draw content based on the current screen
        {
            case MENU:
            {
                // --- Layout Calculations for 3-column menu ---
                int btnWidth = 400;     // Width of each menu button
                int btnHeight = 60;     // Height of each menu button
                int spacing = 20;       // Spacing between buttons
                int startY = 150;       // Y position for the main title
                int totalWidth = (3 * btnWidth) + (2 * spacing); // Total width of 3 columns + 2 gaps
                int colMargin = (SCREEN_WIDTH - totalWidth) / 2; // Left/right margin to center the block
                int col1X = colMargin;                           // X position for column 1
                int col2X = col1X + btnWidth + spacing;          // X position for column 2
                int col3X = col2X + btnWidth + spacing;          // X position for column 3
                int menuY = startY + 60;                         // Y position for the first row of buttons
                // --- End Column Layout Calculation ---

                DrawText("Select an Operation", (SCREEN_WIDTH - MeasureText("Select an Operation", 30))/2, startY, 30, DARKGRAY); // Main title

                // --- Column 1: Banking Operations ---
                DrawText("Banking Operations", col1X + (btnWidth - MeasureText("Banking Operations", 20))/2, menuY - 30, 20, GRAY); // Column title
                if (DrawButton((Rectangle){ col1X, menuY + 0*(btnHeight+spacing), btnWidth, btnHeight }, "View All Accounts")) currentScreen = VIEW_ALL_ACCOUNTS;
                
                // V8 FIX: Load data on first entry to APPROVE_LOANS
                if (DrawButton((Rectangle){ col1X, menuY + 1*(btnHeight+spacing), btnWidth, btnHeight }, "Approve Loans")) { 
                    currentScreen = APPROVE_LOANS;  // Change screen
                    statusMessage[0] = '\0';        // Clear status message
                    Logic_LoadAdminLoans();         // Load loan data into memory
                }
                
                if (DrawButton((Rectangle){ col1X, menuY + 2*(btnHeight+spacing), btnWidth, btnHeight }, "Delete Account")) { currentScreen = DELETE_ACCOUNT; statusMessage[0] = '\0'; InitTextBox(&inputAccountBox, inputAccountBox.bounds, false); }
                if (DrawButton((Rectangle){ col1X, menuY + 3*(btnHeight+spacing), btnWidth, btnHeight }, "Freeze/Unfreeze Account")) { currentScreen = BLOCK_UNBLOCK_CARD; statusMessage[0] = '\0'; InitTextBox(&inputAccountBox, inputAccountBox.bounds, false); }
                if (DrawButton((Rectangle){ col1X, menuY + 4*(btnHeight+spacing), btnWidth, btnHeight }, "Search Account")) {
                    currentScreen = SEARCH_ACCOUNT;
                    statusMessage[0] = '\0';
                    InitTextBox(&inputAccountBox, inputAccountBox.bounds, false);
                    memset(&searchResultAccount, 0, sizeof(searchResultAccount)); // Clear previous search result
                }
                if (DrawButton((Rectangle){ col1X, menuY + 5*(btnHeight+spacing), btnWidth, btnHeight }, "View Loan Status")) { currentScreen = VIEW_LOAN_STATUS; scrollPosition = (Vector2){0,0}; } // Reset scroll

                // --- Column 2: Staff Operations ---
                DrawText("Staff Operations", col2X + (btnWidth - MeasureText("Staff Operations", 20))/2, menuY - 30, 20, GRAY); // Column title
                if (DrawButton((Rectangle){ col2X, menuY + 0*(btnHeight+spacing), btnWidth, btnHeight }, "View All Staff")) currentScreen = VIEW_ALL_STAFF; 
                if (DrawButton((Rectangle){ col2X, menuY + 1*(btnHeight+spacing), btnWidth, btnHeight }, "Search Staff Profile")) { currentScreen = SEARCH_STAFF; statusMessage[0] = '\0'; InitTextBox(&inputStaffSearchBox, inputStaffSearchBox.bounds, false); }
                if (DrawButton((Rectangle){ col2X, menuY + 2*(btnHeight+spacing), btnWidth, btnHeight }, "Create Staff Account")) {
                    currentScreen = CREATE_STAFF;
                    statusMessage[0] = '\0';
                    InitTextBox(&inputStaffIdBox, inputStaffIdBox.bounds, false);   // Reset staff ID box
                    InitTextBox(&inputStaffPassBox, inputStaffPassBox.bounds, true); // Reset staff password box
                }
                if (DrawButton((Rectangle){ col2X, menuY + 3*(btnHeight+spacing), btnWidth, btnHeight }, "View Staff Changelog")) { currentScreen = VIEW_CHANGELOG; scrollPosition = (Vector2){0,0}; } // Reset scroll

                // --- Column 3: Care Analytics ---
                DrawText("Care Analytics", col3X + (btnWidth - MeasureText("Care Analytics", 20))/2, menuY - 30, 20, GRAY); // Column title
                if (DrawButton((Rectangle){ col3X, menuY + 0*(btnHeight+spacing), btnWidth, btnHeight }, "View All Ratings")) { currentScreen = VIEW_ALL_RATINGS; scrollPosition = (Vector2){0,0}; } // Reset scroll
                if (DrawButton((Rectangle){ col3X, menuY + 1*(btnHeight+spacing), btnWidth, btnHeight }, "View All Complaints")) { currentScreen = VIEW_ALL_COMPLAINTS; scrollPosition = (Vector2){0,0}; } // Reset scroll
                // Removed old complaint buttons

            } break;

            case SEARCH_ACCOUNT: // Screen to search for a customer account
            {
                float centerX = SCREEN_WIDTH/2;
                Rectangle searchButton = { centerX - 100, 380, 200, 60 }; // Search button position

                DrawText("Search Account", (SCREEN_WIDTH - MeasureText("Search Account", 40))/2, 200, 40, (Color){ 60, 90, 150, 255 });
                DrawBackButton(); // Draw the back button
                DrawTextBox(&inputAccountBox, "Enter Account No."); // Draw the input box

                if (DrawButton(searchButton, "SEARCH")) { // If search button is clicked
                    int accNo = atoi(inputAccountBox.text); // Convert text input to integer
                    if (accNo > 0) {
                        searchResultAccount = getStaffAccount(accNo); // Fetch account details from accounts.csv
                        if (searchResultAccount.accountNumber != 0) {
                            sprintf(statusMessage, "Success: Account %d found.", accNo); // Found
                        } else {
                            sprintf(statusMessage, "Error: Account %d not found.", accNo); // Not found
                            memset(&searchResultAccount, 0, sizeof(searchResultAccount)); // Clear struct
                        }
                    } else {
                        strcpy(statusMessage, "Error: Please enter a valid account number."); // Invalid input
                        memset(&searchResultAccount, 0, sizeof(searchResultAccount)); // Clear struct
                    }
                }

                // Draw status message (green for success, red for error)
                DrawText(statusMessage, (SCREEN_WIDTH - MeasureText(statusMessage, 20))/2, 460, 20, (statusMessage[0] == 'S') ? GREEN : RED);

                // --- Display Results ---
                if (searchResultAccount.accountNumber != 0) { // Only draw if an account was found
                    float detailsY = 500; // Starting Y for results
                    float detailsX = centerX - 250; // Starting X for results
                    int lineHeight = 25; // Line height

                    DrawText("Account Details:", detailsX, detailsY, 25, (Color){ 60, 90, 150, 255 }); // Title for results
                    detailsY += lineHeight * 2; // Add spacing

                    // Draw all account details
                    DrawText(TextFormat("Account Number: %d", searchResultAccount.accountNumber), detailsX, detailsY, 20, DARKGRAY);
                    detailsY += lineHeight;
                    DrawText(TextFormat("Account Name: %s", searchResultAccount.name), detailsX, detailsY, 20, DARKGRAY);
                    detailsY += lineHeight;
                    DrawText(TextFormat("PIN: %s", searchResultAccount.pin), detailsX, detailsY, 20, DARKGRAY); // Note: Displaying PIN (as requested by design)
                    detailsY += lineHeight;
                    DrawText(TextFormat("Current Balance: $%.2f", searchResultAccount.balance), detailsX, detailsY, 20, DARKGRAY);
                    detailsY += lineHeight;
                    DrawText(TextFormat("Card Number: %s", searchResultAccount.cardNumber), detailsX, detailsY, 20, DARKGRAY);
                    detailsY += lineHeight;
                    DrawText(TextFormat("Account Status: %s", searchResultAccount.cardBlocked ? "FROZEN" : "ACTIVE"), detailsX, detailsY, 20, (searchResultAccount.cardBlocked ? RED : GREEN)); // Color-coded status
                    detailsY += lineHeight;
                    DrawText(TextFormat("Created By Staff: %s", searchResultAccount.createdBy), detailsX, detailsY, 20, DARKGRAY);
                }
            } break;

            case VIEW_ALL_STAFF: // Screen to view all staff accounts
            {
                // Column X positions for symmetric display
                #define S_STAFF_ID  60
                #define S_PASSWORD  300

                DrawText("All Staff Profiles", 50, 100, 30, (Color){ 60, 90, 150, 255 }); // Title
                DrawBackButton(); // Back button

                FILE *fp = fopen(STAFF_FILE, "r"); // Open staff.txt for reading
                if (!fp) {
                    DrawText("Staff file (staff.txt) not found!", 50, 150, 20, RED);
                } else {
                    char fileStaffId[MAX_INPUT_CHARS + 1];
                    char filePass[MAX_INPUT_CHARS + 1];
                    int y = 180 + (int)scrollPosition.y; // Initial Y pos, adjusted by scroll
                    int lineHeight = 30; // Height of each line

                    DrawRectangle(40, 140, SCREEN_WIDTH-80, 35, LIGHTGRAY); // Header background
                    // Draw Headers
                    DrawText("Staff ID", S_STAFF_ID, 148, 20, DARKGRAY);
                    DrawText("Password (Hidden)", S_PASSWORD, 148, 20, DARKGRAY);

                    BeginScissorMode(0, 175, SCREEN_WIDTH, SCREEN_HEIGHT - 175); // Start scrollable area
                        // Read in 2-column format (ID password)
                        while (fscanf(fp, "%s %s", fileStaffId, filePass) == 2) { // Read file line by line
                             // Draw data at the defined X coordinates
                             DrawText(fileStaffId, S_STAFF_ID, y, 20, BLACK);
                             DrawText("********", S_PASSWORD, y, 20, BLACK); // Display masked password
                             y += lineHeight; // Move to next line
                        }
                    EndScissorMode(); // End scrollable area
                    fclose(fp); // Close the file
                }
            } break;

            case SEARCH_STAFF: // Screen to search for a staff profile
            {
                DrawText("Search Staff Profile", (SCREEN_WIDTH - MeasureText("Search Staff Profile", 40))/2, 200, 40, (Color){ 60, 90, 150, 255 }); // Title
                DrawBackButton(); // Back button
                DrawTextBox(&inputStaffSearchBox, "Enter Staff ID to Search"); // Input box

                Rectangle searchButton = { SCREEN_WIDTH/2 - 100, 380, 200, 60 }; // Search button

                if (DrawButton(searchButton, "SEARCH")) { // On click
                    if (strlen(inputStaffSearchBox.text) > 0) {
                        // Logic_StaffExists checks if the ID is in staff.txt
                        if (Logic_StaffExists(inputStaffSearchBox.text)) {
                            sprintf(statusMessage, "Success: Staff ID '%s' found!", inputStaffSearchBox.text); // Found
                        } else {
                            sprintf(statusMessage, "Error: Staff ID '%s' not found!", inputStaffSearchBox.text); // Not found
                        }
                    } else {
                        strcpy(statusMessage, "Error: Staff ID cannot be empty."); // Empty input
                    }
                }
                // Draw status message (green/red)
                DrawText(statusMessage, (SCREEN_WIDTH - MeasureText(statusMessage, 20))/2, 460, 20, (statusMessage[0] == 'S') ? GREEN : RED);
            } break;


            case VIEW_CHANGELOG: // Screen to view staff changelog
            {
                // --- Column X positions for symmetric display ---
                #define C_STAFF_ID  60
                #define C_ACTION    280 // Wider space for Action description
                #define C_ACCNO     650 // Account number
                #define C_AMOUNT    850 // Amount
                #define C_TIMESTAMP 1150 // Timestamp (needs the most room)
                // --- End Column Definitions ---

                DrawText("Staff Changelog", 50, 100, 30, (Color){ 60, 90, 150, 255 }); // Title
                DrawBackButton(); // Back button

                FILE *fp = fopen(CHANGELOG_FILE, "r"); // Open changelog.csv
                if (!fp) {
                    DrawText("No changelog file (changelog.csv) found!", 50, 150, 20, RED);
                } else {
                    char line[1024]; // Buffer for one line
                    int y = 180 + (int)scrollPosition.y; // Y pos with scroll
                    int lineHeight = 30; // Line height

                    DrawRectangle(40, 140, SCREEN_WIDTH-80, 35, LIGHTGRAY); // Header background

                    // Draw Headers aligned with data columns
                    DrawText("Staff ID", C_STAFF_ID, 148, 20, DARKGRAY);
                    DrawText("Action", C_ACTION, 148, 20, DARKGRAY);
                    DrawText("Acc. No", C_ACCNO, 148, 20, DARKGRAY);
                    DrawText("Amount", C_AMOUNT, 148, 20, DARKGRAY);
                    DrawText("Timestamp", C_TIMESTAMP, 148, 20, DARKGRAY);

                    BeginScissorMode(0, 175, SCREEN_WIDTH, SCREEN_HEIGHT - 175); // Start scroll area
                        while (fgets(line, sizeof(line), fp)) { // Read line by line
                             char staffId[50]={0}, action[50]={0}, accNo[20]={0}, amount[20]={0}, timestamp[64]={0}; // Buffers for parsed data

                             // Parse the CSV line (5 columns expected)
                             if (sscanf(line, "%49[^,],%49[^,],%19[^,],%19[^,],%63[^\n]",
                                        staffId, action, accNo, amount, timestamp) == 5)
                             {
                                 // Draw each data point at the defined X coordinate
                                 DrawText(staffId, C_STAFF_ID, y, 20, BLACK);
                                 DrawText(action, C_ACTION, y, 20, BLACK);
                                 DrawText(accNo, C_ACCNO, y, 20, BLACK);
                                 DrawText(amount, C_AMOUNT, y, 20, BLACK);
                                 DrawText(timestamp, C_TIMESTAMP, y, 20, BLACK);
                                 y += lineHeight; // Move to next line
                             }
                        }
                    EndScissorMode(); // End scroll area
                    fclose(fp); // Close file
                }
            } break;

            case CREATE_STAFF: // Screen to create a new staff account
            {
                float centerX = SCREEN_WIDTH/2;
                Rectangle actionButton = { centerX - 100, 300 + 70*2, 200, 60 }; // Button position

                DrawText("Create New Staff Account", (SCREEN_WIDTH - MeasureText("Create New Staff Account", 40))/2, 200, 40, (Color){ 60, 90, 150, 255 }); // Title
                DrawBackButton(); // Back button

                DrawTextBox(&inputStaffIdBox, "Enter Staff ID (e.g., staff02)"); // Staff ID input
                DrawTextBox(&inputStaffPassBox, "Enter Password"); // Staff password input (masked)

                if (DrawButton(actionButton, "CREATE STAFF")) { // On click
                    if (strlen(inputStaffIdBox.text) > 0 && strlen(inputStaffPassBox.text) > 0) { // Check for empty fields
                        Logic_CreateStaffAccount(inputStaffIdBox.text, inputStaffPassBox.text); // Call logic function
                    } else {
                        strcpy(statusMessage, "Error: Staff ID and Password cannot be empty."); // Set error
                    }
                }
                // Draw status message (green/red)
                DrawText(statusMessage, (SCREEN_WIDTH - MeasureText(statusMessage, 20))/2, actionButton.y + 80, 20, (statusMessage[0] == 'S') ? GREEN : RED);
            } break;

            case VIEW_ALL_ACCOUNTS: // Screen to view all customer accounts
            {
                DrawText("All Accounts", 50, 100, 30, (Color){ 60, 90, 150, 255 }); // Title
                DrawBackButton(); // Back button

                FILE *fp = fopen(FILE_NAME, "r"); // Open accounts.csv
                if (!fp) {
                    DrawText("No accounts file found!", 50, 150, 20, RED);
                } else {
                    char line[1024]; // Line buffer
                    int y = 180 + (int)scrollPosition.y; // Y pos with scroll
                    int lineHeight = 30; // Line height

                    DrawRectangle(40, 140, SCREEN_WIDTH-80, 35, LIGHTGRAY); // Header background
                    DrawText("AccNo   Name                 Balance      Status     CreatedBy", 50, 148, 20, DARKGRAY); // Headers

                    BeginScissorMode(0, 175, SCREEN_WIDTH, SCREEN_HEIGHT - 175); // Start scroll area
                        while (fgets(line, sizeof(line), fp)) { // Read line by line
                             char accNo[20]={0}, name[100]={0}, pin[10]={0}, bal[20]={0}, blocked[5]={0}, card[24]={0}, createdBy[50]={0}; // Buffers
                             sscanf(line, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^\n]", accNo, name, pin, bal, blocked, card, createdBy); // Parse CSV
                             char displayLine[256]; // Buffer for formatted line
                             // Format the line with fixed spacing
                             sprintf(displayLine, "%-8s %-20s %-12s %-10s %s", accNo, name, bal, (atoi(blocked) ? "Frozen" : "Active"), createdBy);
                             DrawText(displayLine, 50, y, 20, BLACK); // Draw the line
                             y += lineHeight; // Move to next line
                        }
                    EndScissorMode(); // End scroll area
                    fclose(fp); // Close file
                }
            } break;

            case DELETE_ACCOUNT: // Screen to delete a customer account
            {
                DrawText("Delete Account", (SCREEN_WIDTH - MeasureText("Delete Account", 40))/2, 200, 40, (Color){ 60, 90, 150, 255 }); // Title
                DrawBackButton(); // Back button
                DrawTextBox(&inputAccountBox, "Enter Account No. to Delete"); // Input box

                if (DrawButton((Rectangle){ SCREEN_WIDTH/2 - 100, 380, 200, 60 }, "DELETE")) { // On click
                    if (strlen(inputAccountBox.text) > 0) {
                        Logic_DeleteAccount(atoi(inputAccountBox.text)); // Call logic function
                    }
                }
                // Draw status message (green/red)
                DrawText(statusMessage, (SCREEN_WIDTH - MeasureText(statusMessage, 20))/2, 460, 20, (statusMessage[0] == 'S') ? GREEN : RED);
            } break;

            case BLOCK_UNBLOCK_CARD: // Screen to freeze/unfreeze an account
            {
                 DrawText("Freeze/Unfreeze Account", (SCREEN_WIDTH - MeasureText("Freeze/Unfreeze Account", 40))/2, 200, 40, (Color){ 60, 90, 150, 255 }); // Title
                 DrawBackButton(); // Back button
                 DrawTextBox(&inputAccountBox, "Enter Account No."); // Input box

                 if (DrawButton((Rectangle){ SCREEN_WIDTH/2 - 125, 380, 250, 60 }, "FREEZE/UNFREEZE")) { // On click
                     if (strlen(inputAccountBox.text) > 0) {
                         Logic_BlockUnblockCard(atoi(inputAccountBox.text)); // Call logic function
                     }
                 }
                 // Draw status message (green/red)
                 DrawText(statusMessage, (SCREEN_WIDTH - MeasureText(statusMessage, 20))/2, 460, 20, (statusMessage[0] == 'S') ? GREEN : RED);
            } break;

            //
            // --- V8 FIX: THIS ENTIRE CASE IS RE-WRITTEN ---
            //
            case APPROVE_LOANS: // Screen to approve/reject loans
            {
                DrawText("Approve Pending Loans", 50, 100, 30, (Color){ 60, 90, 150, 255 }); // Title
                
                // Draw custom back button in bottom-right corner
                if (DrawButton((Rectangle){ SCREEN_WIDTH - 180, SCREEN_HEIGHT - 70, 150, 40 }, "Back to Menu")) {
                    currentScreen = MENU;
                    statusMessage[0] = '\0';
                    scrollPosition = (Vector2){0,0};
                    memset(&searchResultAccount, 0, sizeof(searchResultAccount)); // Clear search
                    adminLoanCount = 0; // V8 FIX: Clear loan array on exit
                }

                // Draw status message (bottom-left)
                DrawText(statusMessage, 50, SCREEN_HEIGHT - 60, 20, (statusMessage[0] == 'S') ? GREEN : DARKGRAY);

                if (adminLoanCount == 0) { // If no loans were loaded
                    DrawText("No loans found.", 50, 180, 20, GRAY);
                } else {
                    int y = 180 + (int)scrollPosition.y; // Y pos with scroll
                    int itemHeight = 90; // Height of each loan item

                    DrawRectangle(40, 140, SCREEN_WIDTH-80, 35, LIGHTGRAY); // Header background
                    DrawText("ID    Acc.No   Amount      Tenure   Rate   EMI      Status/Actions", 50, 148, 20, DARKGRAY); // Headers

                    BeginScissorMode(0, 175, SCREEN_WIDTH, SCREEN_HEIGHT - 220); // Start scroll area
                        
                        // Loop from the in-memory array (loaded on screen entry)
                        for (int i = 0; i < adminLoanCount; i++) {
                            struct Loan l = adminLoanList[i]; // Get loan from array

                            char displayLine[256]; // Format loan details
                            sprintf(displayLine, "%-5d %-8d %-11.2f %-8d %-6.2f %-8.2f",
                                    l.loan_id, l.acc_no, l.amount, l.tenure, l.rate, l.emi);

                            DrawText(displayLine, 50, y + 10, 20, BLACK); // Draw details

                            // Conditional drawing for actions:
                            if (strcmp(l.status, "Pending") == 0) { // If loan is pending
                                // Draw buttons
                                Rectangle approveBtn = { SCREEN_WIDTH - 280, y, 100, 30 };
                                Rectangle rejectBtn = { SCREEN_WIDTH - 170, y, 100, 30 };

                                if (DrawButton(approveBtn, "Approve")) { // On approve click
                                    Logic_UpdateLoanStatus(l.loan_id, "Approved"); // Call logic
                                    needsRefresh = true; // Set flag to reload data
                                    break; // Break from loop to allow refresh
                                }
                                if (DrawButton(rejectBtn, "Reject")) { // On reject click
                                    Logic_UpdateLoanStatus(l.loan_id, "Rejected"); // Call logic
                                    needsRefresh = true; // Set flag to reload data
                                    break; // Break from loop to allow refresh
                                }
                            } else { // If loan is already Approved or Rejected
                                // Draw the status as text instead of buttons
                                Color statusColor = GRAY;
                                if (strcmp(l.status, "Approved") == 0) statusColor = GREEN;
                                else if (strcmp(l.status, "Rejected") == 0) statusColor = RED;
                                DrawText(l.status, SCREEN_WIDTH - 280, y + 5, 20, statusColor);
                            }

                            DrawRectangle(40, y + itemHeight - 2, SCREEN_WIDTH - 80, 2, LIGHTGRAY); // Divider line
                            y += itemHeight; // Move to next item
                        }
                    EndScissorMode(); // End scroll area
                }
            } break;
            // --- END V8 FIX ---

            case VIEW_LOAN_STATUS: // Read-only screen for all loans
            {
                // --- Column X positions for symmetric display ---
                #define L_LOAN_ID 60
                #define L_ACC_NO  180
                #define L_TYPE    300
                #define L_AMOUNT  420
                #define L_RATE    580
                #define L_TENURE  700
                #define L_EMI     820
                #define L_STATUS  1000
                // --- End Column Definitions ---

                DrawText("Loan Status Overview", 50, 100, 30, (Color){ 60, 90, 150, 255 }); // Title
                DrawBackButton(); // Back button

                FILE *fp = fopen(LOAN_FILE, "r"); // Open temp_loan.txt
                if (!fp) {
                    DrawText("No loan file (temp_loan.txt) found!", 50, 150, 20, RED);
                } else {
                    char line[1024]; // Line buffer
                    int y = 180 + (int)scrollPosition.y; // Y pos with scroll
                    int lineHeight = 30; // Line height

                    DrawRectangle(40, 140, SCREEN_WIDTH-80, 35, LIGHTGRAY); // Header background

                    // Draw Headers aligned with data columns
                    DrawText("Loan ID", L_LOAN_ID, 148, 20, DARKGRAY);
                    DrawText("Acc. No", L_ACC_NO, 148, 20, DARKGRAY);
                    DrawText("Type", L_TYPE, 148, 20, DARKGRAY);
                    DrawText("Amount", L_AMOUNT, 148, 20, DARKGRAY);
                    DrawText("Rate (%)", L_RATE, 148, 20, DARKGRAY);
                    DrawText("Tenure (M)", L_TENURE, 148, 20, DARKGRAY);
                    DrawText("EMI", L_EMI, 148, 20, DARKGRAY);
                    DrawText("Status", L_STATUS, 148, 20, DARKGRAY);

                    BeginScissorMode(0, 175, SCREEN_WIDTH, SCREEN_HEIGHT - 175); // Start scroll area
                        while (fgets(line, sizeof(line), fp)) { // Read line by line
                            struct Loan l; // Struct to hold parsed data
                            // Parse the 8-column loan format
                            if (sscanf(line, "%d %d %d %f %f %d %f %19s",
                                       &l.loan_id, &l.acc_no, &l.type, &l.amount,
                                       &l.rate, &l.tenure, &l.emi, l.status) != 8) continue; // Skip bad lines

                            // Draw each data point at the defined X coordinate
                            DrawText(TextFormat("%d", l.loan_id), L_LOAN_ID, y, 20, BLACK);
                            DrawText(TextFormat("%d", l.acc_no), L_ACC_NO, y, 20, BLACK);

                            // Display loan type (integer) as text
                            const char* loanTypeStr = "Unknown";
                            if (l.type == 1) loanTypeStr = "Home";
                            else if (l.type == 2) loanTypeStr = "Car";
                            else if (l.type == 3) loanTypeStr = "Gold";
                            else if (l.type == 4) loanTypeStr = "Personal";
                            else if (l.type == 5) loanTypeStr = "Business";
                            else if (l.type == 6) loanTypeStr = "Education";
                            DrawText(loanTypeStr, L_TYPE, y, 20, BLACK);

                            DrawText(TextFormat("$%.2f", l.amount), L_AMOUNT, y, 20, BLACK);
                            DrawText(TextFormat("%.2f%%", l.rate), L_RATE, y, 20, BLACK);
                            DrawText(TextFormat("%d", l.tenure), L_TENURE, y, 20, BLACK);
                            DrawText(TextFormat("$%.2f", l.emi), L_EMI, y, 20, BLACK);

                            // Color code the status text
                            Color statusColor = BLACK;
                            if (strcmp(l.status, "Approved") == 0) statusColor = (Color){ 0, 150, 0, 255 }; // Dark Green
                            else if (strcmp(l.status, "Rejected") == 0) statusColor = RED;
                            else if (strcmp(l.status, "Pending") == 0) statusColor = (Color){ 200, 120, 0, 255 }; // Orange
                            DrawText(l.status, L_STATUS, y, 20, statusColor);

                            y += lineHeight; // Move to next line
                        }
                    EndScissorMode(); // End scroll area
                    fclose(fp); // Close file
                }
            } break;

            case VIEW_ALL_RATINGS: // Screen to view all customer ratings
            {
                // --- Column X positions for symmetric display ---
                #define R_ACC_NO  60
                #define R_RATING  200
                #define R_COMMENT 350
                // --- End Column Definitions ---

                DrawText("All Customer Ratings", 50, 100, 30, (Color){ 60, 90, 150, 255 }); // Title
                DrawBackButton(); // Back button

                FILE *fp = fopen(RATING_FILE, "r"); // Open ratings.csv
                if (!fp) {
                    DrawText("No ratings file (ratings.csv) found!", 50, 150, 20, RED);
                } else {
                    char line[1024]; // Line buffer
                    int y = 180 + (int)scrollPosition.y; // Y pos with scroll
                    int lineHeight = 30; // Line height

                    DrawRectangle(40, 140, SCREEN_WIDTH-80, 35, LIGHTGRAY); // Header background

                    // Draw Headers aligned with data columns
                    DrawText("Acc. No", R_ACC_NO, 148, 20, DARKGRAY);
                    DrawText("Rating (1-5)", R_RATING, 148, 20, DARKGRAY);
                    DrawText("Comment", R_COMMENT, 148, 20, DARKGRAY);

                    BeginScissorMode(0, 175, SCREEN_WIDTH, SCREEN_HEIGHT - 175); // Start scroll area
                        while (fgets(line, sizeof(line), fp)) { // Read line by line
                             char r_accNo[20]={0}, r_rating[10]={0}, r_comment[512]={0}; // Buffers

                             // Assuming format: AccountNo,Rating,Comment
                             if (sscanf(line, "%19[^,],%9[^,],%511[^\n]",
                                        r_accNo, r_rating, r_comment) == 3) // Parse 3-column CSV
                             {
                                 // Draw data at defined X positions
                                 DrawText(r_accNo, R_ACC_NO, y, 20, BLACK);
                                 DrawText(r_rating, R_RATING, y, 20, BLACK);
                                 DrawText(r_comment, R_COMMENT, y, 20, BLACK);
                                 y += lineHeight; // Move to next line
                             }
                        }
                    EndScissorMode(); // End scroll area
                    fclose(fp); // Close file
                }
            } break;

            case VIEW_ALL_COMPLAINTS: // Screen to view all customer complaints
            {
                // --- Column X positions for symmetric display ---
                #define CP_ID       60
                #define CP_ACC_NO   220 // Increased spacing
                #define CP_STATUS   380 // Increased spacing
                #define CP_COMPLAINT 540 // Increased spacing
                // --- End Column Definitions ---

                DrawText("All Complaints", 50, 100, 30, (Color){ 60, 90, 150, 255 }); // Title
                DrawBackButton(); // Back button

                FILE *fp = fopen(COMPLAINT_FILE, "r"); // Open complaints.csv
                if (!fp) {
                    DrawText("No complaints file (complaints.csv) found!", 50, 150, 20, RED);
                } else {
                    char line[1024]; // Line buffer
                    int y = 180 + (int)scrollPosition.y; // Y pos with scroll
                    int lineHeight = 30; // Line height

                    DrawRectangle(40, 140, SCREEN_WIDTH-80, 35, LIGHTGRAY); // Header background

                    // Draw Headers aligned with data columns
                    DrawText("Complaint ID", CP_ID, 148, 20, DARKGRAY);
                    DrawText("Acc. No", CP_ACC_NO, 148, 20, DARKGRAY);
                    DrawText("Status", CP_STATUS, 148, 20, DARKGRAY);
                    DrawText("Details", CP_COMPLAINT, 148, 20, DARKGRAY);

                    BeginScissorMode(0, 175, SCREEN_WIDTH, SCREEN_HEIGHT - 175); // Start scroll area
                        while (fgets(line, sizeof(line), fp)) { // Read line by line
                            char c_id[50]={0}, c_accNo[20]={0}, c_status[20]={0}, c_complaint[300]={0}; // Buffers

                            // Assuming format: ID,AccountNo,Status,ComplaintText
                            if (sscanf(line, "%49[^,],%19[^,],%19[^,],%299[^\n]",
                                       c_id, c_accNo, c_status, c_complaint) == 4) // Parse 4-column CSV
                            {
                                 // Draw data at defined X positions
                                 DrawText(c_id, CP_ID, y, 20, BLACK);
                                 DrawText(c_accNo, CP_ACC_NO, y, 20, BLACK);
                                 
                                 // Color code the status text
                                Color statusColor = BLACK;
                                if (strcmp(c_status, "Resolved") == 0) statusColor = (Color){ 0, 150, 0, 255 }; // Dark Green
                                else if (strcmp(c_status, "Pending") == 0) statusColor = (Color){ 200, 120, 0, 255 }; // Orange
                                 DrawText(c_status, CP_STATUS, y, 20, statusColor);

                                 DrawText(c_complaint, CP_COMPLAINT, y, 20, BLACK);
                                 y += lineHeight; // Move to next line
                            }
                        }
                    EndScissorMode(); // End scroll area
                    fclose(fp); // Close file
                }
            } break;

            default: // Fallback for any unhandled screen state
                DrawText("Feature Screen Placeholder", (SCREEN_WIDTH - MeasureText("Feature Screen Placeholder", 30))/2, SCREEN_HEIGHT/2, 30, LIGHTGRAY);
                DrawBackButton();
                break;
        }

        EndDrawing(); // End drawing phase
    }

    CloseWindow(); // Close window and free resources
    return 0;
}

//----------------------------------------------------------------------------------
// GUI Helper Functions
//----------------------------------------------------------------------------------
// Initializes/resets a text box struct
static void InitTextBox(TextBox *box, Rectangle bounds, bool isPassword) {
    box->bounds = bounds;                     // Set position and size
    box->charCount = 0;                       // Reset character count
    box->active = false;                      // Set to inactive
    memset(box->text, 0, sizeof(box->text));  // Clear the text buffer
    box->isPassword = isPassword;             // Set password flag
}

// Handles keyboard input for an active text box
static void UpdateTextBox(TextBox *box) {
    if (box->active) { // Only run if the box is active
        int key = GetCharPressed(); // Get character (Unicode)
        while (key > 0) { // Process all keys pressed this frame
            if ((key >= 32) && (box->charCount < MAX_INPUT_CHARS)) { // If printable char and not full
                box->text[box->charCount] = (char)key; // Add char to buffer
                box->text[box->charCount + 1] = '\0';  // Add null terminator
                box->charCount++;                      // Increment count
            }
            key = GetCharPressed(); // Get next key in queue
        }
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) { // If backspace is pressed/held
             if (box->charCount > 0) { // If there's text to delete
                 box->charCount--;                     // Decrement count
                 box->text[box->charCount] = '\0';     // Move null terminator back
             }
        }
    }
}

// Draws the text box on screen
static void DrawTextBox(TextBox *box, const char *placeholder) {
    DrawRectangleRec(box->bounds, WHITE); // Draw white background
    if (box->active) DrawRectangleLinesEx(box->bounds, 2, (Color){ 80, 120, 200, 255 }); // Thick blue border if active
    else DrawRectangleLinesEx(box->bounds, 1, GRAY); // Thin gray border if inactive

    // DRAW TEXT CONTENT (Masking if isPassword is true)
    if (box->charCount > 0) { // If there is text
        if (box->isPassword) { // If it's a password
            char passwordStars[MAX_INPUT_CHARS + 1] = { 0 }; // Buffer for stars
            for(int i = 0; i < box->charCount; i++) passwordStars[i] = '*'; // Fill with stars
            DrawText(passwordStars, box->bounds.x + 10, box->bounds.y + 15, 20, BLACK); // Draw stars
        } else {
            DrawText(box->text, box->bounds.x + 10, box->bounds.y + 15, 20, BLACK); // Draw the real text
        }
    }
    else DrawText(placeholder, box->bounds.x + 10, box->bounds.y + 15, 20, LIGHTGRAY); // Draw placeholder if empty

    // DRAW CURSOR (Handling password length for cursor position)
    if (box->active)
    {
        if (((int)(GetTime() * 2.0f)) % 2 == 0) // Blinking cursor logic
        {
            float textWidth;
            if (box->isPassword) // If password
            {
                char passwordStars[MAX_INPUT_CHARS + 1] = { 0 };
                for(int i = 0; i < box->charCount; i++) passwordStars[i] = '*';
                textWidth = MeasureText(passwordStars, 20); // Measure width of stars
            }
            else // If not password
            {
                textWidth = MeasureText(box->text, 20); // Measure width of text
            }
            DrawRectangle(box->bounds.x + 10 + textWidth, box->bounds.y + 10, 2, box->bounds.height - 20, (Color){ 50, 50, 50, 255 }); // Draw cursor
        }
    }
}

// Draws a button and returns true if clicked
static bool DrawButton(Rectangle bounds, const char *text) {
    bool clicked = false;
    Color bgColor = (Color){ 80, 120, 200, 255 }; // Normal blue
    if (CheckCollisionPointRec(GetMousePosition(), bounds)) { // If mouse is hovering
        bgColor = (Color){ 100, 150, 230, 255 }; // Lighter blue
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { // If clicked
            clicked = true;
            bgColor = (Color){ 60, 90, 150, 255 }; // Darker blue
        }
    }
    DrawRectangleRounded(bounds, 0.2f, 4, bgColor); // Draw button rectangle
    int textW = MeasureText(text, 20); // Measure text
    DrawText(text, bounds.x + (bounds.width - textW)/2, bounds.y + (bounds.height - 20)/2, 20, WHITE); // Draw centered text
    return clicked; // Return click state
}

// Draws the standard "Back to Menu" button
static void DrawBackButton(void) {
    if (DrawButton((Rectangle){ 30, SCREEN_HEIGHT - 70, 150, 40 }, "Back to Menu")) {
        currentScreen = MENU; // Go to menu
        statusMessage[0] = '\0'; // Clear status message
        scrollPosition = (Vector2){0,0}; // Reset scroll
        memset(&searchResultAccount, 0, sizeof(searchResultAccount)); // Clear search result
        adminLoanCount = 0; // V8 FIX: Clear loan array on exit
    }
}

//----------------------------------------------------------------------------------
// Logic Functions
//----------------------------------------------------------------------------------

// V8 FIX: New function to load loans from temp_loan.txt into the global adminLoanList array
static void Logic_LoadAdminLoans(void) {
    adminLoanCount = 0; // Reset count
    FILE *fp = fopen(LOAN_FILE, "r"); // Open for reading
    if (!fp) return; // File not found, do nothing

    char line[1024];
    while (fgets(line, sizeof(line), fp)) { // Read line by line
        if (adminLoanCount >= MAX_ADMIN_LOANS) break; // Stop if array is full

        struct Loan l; // Temporary struct to parse into
        if (sscanf(line, "%d %d %d %f %f %d %f %19s", // Parse 8-column format
                    &l.loan_id, &l.acc_no, &l.type, &l.amount,
                    &l.rate, &l.tenure, &l.emi, l.status) == 8) {
            adminLoanList[adminLoanCount] = l; // Add to array
            adminLoanCount++; // Increment count
        }
    }
    fclose(fp); // File is now closed, lock is released
}


// Checks if a staff ID exists in staff.txt
bool Logic_StaffExists(const char *staffId) {
    FILE *fp = fopen(STAFF_FILE, "r"); // Open for reading
    if (!fp) return false; // File not found

    char fileStaffId[MAX_INPUT_CHARS + 1];
    char filePass[MAX_INPUT_CHARS + 1];

    // Read in 2-column format (ID password)
    while (fscanf(fp, "%s %s", fileStaffId, filePass) == 2) {
        if (strcmp(fileStaffId, staffId) == 0) { // If ID matches
            fclose(fp);
            return true; // Found
        }
    }
    fclose(fp);
    return false; // Not found
}

// Creates a new staff account and saves to staff.txt
void Logic_CreateStaffAccount(const char *staffId, const char *password) {
    if (Logic_StaffExists(staffId)) { // Check if ID already exists
        strcpy(statusMessage, "Error: Staff ID already exists!");
        return;
    }

    FILE *fp = fopen(STAFF_FILE, "a"); // Open for appending
    if (!fp) {
        strcpy(statusMessage, "Error: Could not open staff.txt for writing.");
        return;
    }

    // Write in the required 2-column format: username password
    fprintf(fp, "%s %s\n", staffId, password);
    fclose(fp);

    // Clear inputs and set success message
    InitTextBox(&inputStaffIdBox, inputStaffIdBox.bounds, false);
    InitTextBox(&inputStaffPassBox, inputStaffPassBox.bounds, true);
    sprintf(statusMessage, "Success: Staff account '%s' created and saved to staff.txt.", staffId);
}

// Checks if a customer account exists in accounts.csv
bool Logic_AccountExists(int accNo) {
    FILE *fp = fopen(FILE_NAME, "r"); // Open for reading
    if (!fp) return false; // File not found
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        if (atoi(line) == accNo) { fclose(fp); return true; } // Check if the line starts with the account number
    }
    fclose(fp);
    return false; // Not found
}

// Deletes a customer account from accounts.csv
void Logic_DeleteAccount(int accNo) {
    if (!Logic_AccountExists(accNo)) { strcpy(statusMessage, "Error: Account not found!"); return; } // Check first
    
    // Read from accounts.csv, write to temp.csv (excluding the deleted account)
    FILE *fp = fopen(FILE_NAME, "r"), *temp = fopen("temp.csv", "w");
    if (!fp || !temp) return; // File error
    char line[1024];
    while (fgets(line, sizeof(line), fp)) { // Read line by line
        if (atoi(line) != accNo) fputs(line, temp); // Write line to temp if it's NOT the account to delete
    }
    fclose(fp); fclose(temp);
    remove(FILE_NAME); // Delete original
    rename("temp.csv", FILE_NAME); // Rename temp to original
    strcpy(statusMessage, "Success: Account deleted.");
}

// V3 FIX: Rewrites users.txt with updated block status for one account
bool Logic_UpdateUsersFile(const char *accNoStr, bool isBlocked)
{
    Account accounts[MAX_ACCOUNTS]; // In-memory array for users.txt
    int count = 0;
    FILE *file = fopen(USER_FILE, "r"); // Open users.txt for reading
    if (file == NULL) return false;

    int isBlockedInt;
    // Read all accounts from users.txt into the array
    while (fscanf(file, "%s %s %lf %d", accounts[count].username, accounts[count].password, &accounts[count].balance, &isBlockedInt) != EOF)
    {
        accounts[count].isCardBlocked = (isBlockedInt == 1); // Convert int to bool
        if (strcmp(accounts[count].username, accNoStr) == 0) // If this is the account we're changing
        {
            accounts[count].isCardBlocked = isBlocked; // Apply the change
        }
        count++;
        if (count >= MAX_ACCOUNTS) break; // Stop if array is full
    }
    fclose(file);

    file = fopen(USER_FILE, "w"); // Re-open users.txt for writing (truncates)
    if (file == NULL) return false;
    // Write the entire array back to the file
    for (int i = 0; i < count; i++)
    {
        fprintf(file, "%s %s %.2f %d\n",
                accounts[i].username,
                accounts[i].password,
                accounts[i].balance,
                accounts[i].isCardBlocked ? 1 : 0); // Convert bool to int (1 or 0)
    }
    fclose(file);
    return true; // Success
}

// Toggles the block/unblock status for a customer account
void Logic_BlockUnblockCard(int accNo) {
    if (!Logic_AccountExists(accNo)) { strcpy(statusMessage, "Error: Account not found!"); return; } // Check first

    StaffAccount acc = getStaffAccount(accNo); // Get all account details from accounts.csv
    if (acc.accountNumber == 0) {
        strcpy(statusMessage, "Error: Could not parse account data.");
        return;
    }

    acc.cardBlocked = !acc.cardBlocked; // Toggle the status (0->1 or 1->0)
    updateStaffAccount(acc); // Write the change back to accounts.csv

    // Now, also write the change to users.txt for login sync
    char accNoStr[20];
    sprintf(accNoStr, "%d", accNo);
    if (Logic_UpdateUsersFile(accNoStr, acc.cardBlocked == 1)) // Call the sync function
    {
        sprintf(statusMessage, "Success: Account %d is now %s. (Synced)", accNo, acc.cardBlocked ? "Frozen" : "Active");
    } else {
        sprintf(statusMessage, "Warning: %s, but users.txt sync failed!", acc.cardBlocked ? "Frozen" : "Active");
    }
}

// Updates a loan's status in temp_loan.txt and handles crediting the account if approved
void Logic_UpdateLoanStatus(int loanId, const char* newStatus)
{
    // --- Backup the file before modification ---
    Logic_BackupLoanFile(); // Create a copy in loan_backup.txt

    // V8 FIX: This function is now safe. LOAN_FILE is not open by the drawing loop.
    FILE *fp = fopen(LOAN_FILE, "r"); // Open original for reading
    FILE *temp = fopen(LOAN_TEMP_FILE, "w"); // Open temp file for writing

    if (!fp || !temp) { // Check for file errors
        if(fp) fclose(fp);
        if(temp) fclose(temp);
        strcpy(statusMessage, "Error: Could not open loan files for update.");
        return;
    }

    char line[1024]; // Line buffer
    float approvedAmount = 0.0f; // To store amount if approved
    int accountToCredit = 0; // To store account number if approved
    int found = 0; // Flag if we found the loan

    while (fgets(line, sizeof(line), fp)) { // Read original line by line
        struct Loan l; // Struct to parse into
        // Try to parse the 8-column format
        if (sscanf(line, "%d %d %d %f %f %d %f %19s",
                   &l.loan_id, &l.acc_no, &l.type, &l.amount,
                   &l.rate, &l.tenure, &l.emi, l.status) != 8) {
            fputs(line, temp); // If unparsed (e.g., header), write to temp as-is
            continue;
        }

        if (l.loan_id == loanId) { // If this is the loan to update
            found = 1;
            // Write the line to temp with the new status
            fprintf(temp, "%d %d %d %.2f %.2f %d %.2f %s\n",
                    l.loan_id, l.acc_no, l.type, l.amount,
                    l.rate, l.tenure, l.emi, newStatus);

            if (strcmp(newStatus, "Approved") == 0) { // If approved
                approvedAmount = l.amount; // Store amount to credit
                accountToCredit = l.acc_no; // Store account to credit
            }
        } else {
            fputs(line, temp); // Not the target loan, write to temp unchanged
        }
    }

    fflush(temp); // Ensure temp file is written to disk
    fclose(fp);   // Close original
    fclose(temp); // Close temp

    if (!found) { // If loan ID was never found
        remove(LOAN_TEMP_FILE); // Delete temp file
        strcpy(statusMessage, "Error: Loan ID not found.");
        return;
    }

    // Replace original with temp (Windows-friendly)
    if (remove(LOAN_FILE) != 0) { // Delete original
        strcpy(statusMessage, "Error: Failed to remove old loan file.");
        remove(LOAN_TEMP_FILE); // Clean up temp file
        return;
    }
    if (rename(LOAN_TEMP_FILE, LOAN_FILE) != 0) { // Rename temp to original
        strcpy(statusMessage, "Error: Failed to rename updated loan file.");
        return;
    }

    // If approved, credit balance in accounts.csv and users.txt
    if (accountToCredit > 0 && approvedAmount > 0.0f) {
        StaffAccount acc = getStaffAccount(accountToCredit); // Get account data from accounts.csv
        if (acc.accountNumber != 0) {
            acc.balance += approvedAmount; // Add loan amount to balance
            updateStaffAccount(acc); // Write updated data back to accounts.csv

            // Also update users.txt balance for sync
            char accNoStr[20];
            sprintf(accNoStr, "%d", acc.accountNumber);

            FILE *ufp = fopen(USER_FILE, "r"); // Open users.txt for reading
            if (ufp) {
                Account accounts[MAX_ACCOUNTS]; // In-memory array
                int count = 0;
                int isBlockedInt;
                // Read all users into memory
                while (count < MAX_ACCOUNTS &&
                       fscanf(ufp, "%50s %50s %lf %d",
                              accounts[count].username,
                              accounts[count].password,
                              &accounts[count].balance,
                              &isBlockedInt) == 4)
                {
                    accounts[count].isCardBlocked = (isBlockedInt == 1);
                    if (strcmp(accounts[count].username, accNoStr) == 0) // If this is the account
                    {
                        accounts[count].balance = acc.balance; // Sync the balance
                    }
                    count++;
                }
                fclose(ufp);

                ufp = fopen(USER_FILE, "w"); // Re-open users.txt for writing
                if (ufp) {
                    // Write all users back to file
                    for (int i = 0; i < count; i++)
                    {
                        fprintf(ufp, "%s %s %.2f %d\n",
                                accounts[i].username,
                                accounts[i].password,
                                accounts[i].balance,
                                accounts[i].isCardBlocked ? 1 : 0);
                    }
                    fclose(ufp);
                }
            }
            // Log the loan deposit
            logTransaction(acc.accountNumber, "Loan Deposit", approvedAmount, acc.balance);
            sprintf(statusMessage, "Success: Loan %d approved. Balance updated.", loanId);
        } else {
            sprintf(statusMessage, "Error: Loan %d approved, but failed to credit account %d!", loanId, accountToCredit);
        }
    } else if (found) { // V8 FIX: Ensure status message is set for non-approval (e.g., rejection)
        sprintf(statusMessage, "Success: Loan %d %s.", loanId, newStatus);
    }
}

// Reads a single customer account's details from accounts.csv
static StaffAccount getStaffAccount(int accNo) {
    FILE *fp = fopen(FILE_NAME, "r"); // Open for reading
    StaffAccount acc;
    memset(&acc, 0, sizeof(acc)); // Initialize struct to zero
    if (!fp) return acc; // File not found, return empty struct
    char line[1024];
    while (fgets(line, sizeof(line), fp)) { // Read line by line
        char copy[1024];
        strcpy(copy, line); // Make a copy for strtok
        char *t = strtok(copy, ","); // Get first token (account number)
        if (!t) continue;
        int id = atoi(t);
        if (id == accNo) { // If this is the account
            // Parse the rest of the CSV line token by token
            char *p = strtok(NULL, ","); if (p) { strncpy(acc.name, p, sizeof(acc.name)-1); acc.name[sizeof(acc.name)-1]='\0'; }
            p = strtok(NULL, ","); if (p) { strncpy(acc.pin, p, sizeof(acc.pin)-1); acc.pin[sizeof(acc.pin)-1]='\0'; }
            p = strtok(NULL, ","); if (p) acc.balance = atof(p); else acc.balance = 0.0;
            p = strtok(NULL, ","); if (p) acc.cardBlocked = atoi(p); else acc.cardBlocked = 0;
            p = strtok(NULL, ","); if (p) { strncpy(acc.cardNumber, p, sizeof(acc.cardNumber)-1); acc.cardNumber[sizeof(acc.cardNumber)-1]='\0'; }
            p = strtok(NULL, "\n"); if (p) { strncpy(acc.createdBy, p, sizeof(acc.createdBy)-1); acc.createdBy[sizeof(acc.createdBy)-1] = '\0'; } // Read until newline
            acc.accountNumber = id; // Set the account number
            break; // Stop searching
        }
    }
    fclose(fp);
    return acc; // Return the populated (or empty) struct
}

// Rewrites a single customer account's details in accounts.csv
static void updateStaffAccount(StaffAccount acc) {
    FILE *fp = fopen(FILE_NAME, "r"); // Open original for reading
    if (!fp) return;
    FILE *temp = fopen("temp.csv", "w"); // Open temp for writing
    if (!temp) { fclose(fp); return; }

    char line[1024];
    while (fgets(line, sizeof(line), fp)) { // Read original line by line
        char copy[1024];
        strcpy(copy, line);
        char *tok = strtok(copy, ","); // Get first token (account number)
        if (!tok) continue;
        int id = atoi(tok);
        if (id == acc.accountNumber) { // If this is the account to update
            // Write the updated data from the struct
            fprintf(temp, "%d,%s,%s,%.2f,%d,%s,%s\n",
                    acc.accountNumber, acc.name, acc.pin,
                    acc.balance, acc.cardBlocked, acc.cardNumber, acc.createdBy);
        } else {
            fputs(line, temp); // Not the target, write original line
        }
    }
    fclose(fp); // Close original
    fclose(temp); // Close temp
    remove(FILE_NAME); // Delete original
    rename("temp.csv", FILE_NAME); // Rename temp to original
}

// Gets the current date/time as a formatted string
static void getCurrentTimestamp(char *buffer, size_t size) {
    time_t now = time(NULL); // Get current time
    struct tm *tm_info = localtime(&now); // Convert to local time struct
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info); // Format as string
}

// Appends a transaction to transactions.csv
static void logTransaction(int accNo, const char *type, double amount, double balance) {
    FILE *fp = fopen(TRANSACTION_FILE, "a"); // Open for appending
    if (!fp) return; // File error
    char timeStr[64];
    getCurrentTimestamp(timeStr, sizeof(timeStr)); // Get timestamp
    // Write transaction as a new CSV line
    fprintf(fp, "%d,%s,%.2f,%.2f,%s\n", accNo, type, amount, balance, timeStr);
    fclose(fp);
}

// Creates a backup of the loan file
static void Logic_BackupLoanFile(void) {
    FILE *src = fopen(LOAN_FILE, "r"); // Open source (temp_loan.txt)
    if (!src) {
        // Source file doesn't exist, nothing to back up.
        return;
    }
    FILE *dest = fopen(LOAN_BACKUP_FILE, "w"); // Open destination (loan_backup.txt)
    if (!dest) {
        fclose(src);
        // Failed to create backup, print to console.
        printf("ADMIN_LOG: Failed to create loan backup file (%s).\n", LOAN_BACKUP_FILE);
        return;
    }

    char buffer[4096]; // Buffer for copying
    size_t n;
    // Copy file contents in chunks
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, n, dest) != n) {
            // Write error
            printf("ADMIN_LOG: Failed to write to loan backup file.\n");
            break;
        }
    }

    fclose(src); // Close source
    fclose(dest); // Close destination
}