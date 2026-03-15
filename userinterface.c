/*******************************************************************************************
*
* User Dashboard (Raylib)
*
* This is a separate application launched from the main login program.
* It receives a username (account number) as a command-line argument
* and provides all user-specific banking features.
*
* Compile example (Windows):
* gcc userinterface.c -o userinterface.exe -lraylib -lgdi32 -lwinmm -Wl,-subsystem,windows
*
* This version is updated to include all features from bank_system_with_care.c
* - Profile, Change PIN, Loan Menu, Care Menu, FAQs, Support
* - Adds PIN verification for sensitive actions.
*
* V3 Changes:
* - Removed PIN check on all buttons except "Change PIN".
* - "Change PIN" now requires OLD PIN.
*
* V4 Changes:
* - Auto-generates Loan ID (starting 10001).
* - Replaced Loan Type textbox with clickable buttons.
* - Loan applications are set to "Pending" for admin approval.
*
* V5 Changes:
* - Fixed blocked account login bug by cross-checking accounts.csv on startup.
* - Moved all message/result text to the bottom-center of the screen.
* - Added a "Refresh" button to the Loan Details screen.
*
* V6 (Gemini Fix) Changes:
* - Changed LOAN_FILE to "temp_loan.txt" to match admin interface.
* - Added balance refresh on main dashboard to reflect admin-side changes.
*
* V7 (User Request) Changes:
* - Replaced Complaint Type textbox with clickable buttons.
*
* V8 (User Request) Changes:
* - Added Recipient Name check and confirmation step to Transfer.
* - Added 2-loan limit check for new loan applications.
*
********************************************************************************************/

#include "raylib.h"     // Core Raylib library for GUI, drawing, and input
#include <stdio.h>      // Standard input/output for file operations (fopen, printf, etc.)
#include <string.h>     // String manipulation functions (strcpy, strcmp, etc.)
#include <stdlib.h>     // Standard library for system(), atoi(), atof()
#include <time.h>       // For generating timestamps
#include <math.h>       // Needed for loan/fd calcs (pow)

//----------------------------------------------------------------------------------
// Defines and Types
//----------------------------------------------------------------------------------
#define SCREEN_WIDTH 1620       // Define the width of the application window
#define SCREEN_HEIGHT 920       // Define the height of the application window

#define MAX_INPUT_CHARS 50      // Standard input size
#define MAX_INPUT_LARGE 500     // For complaint descriptions
#define MAX_INPUT_NAME 100      // For recipient name
#define MAX_ACCOUNTS 100        // Max accounts we can load into memory (for users.txt rewrite)
#define MAX_TRANSACTIONS 50     // Max transactions to show on statement
#define MAX_COMPLAINTS 20       // Max complaints to show for this user
#define MAX_LOANS 20            // Max loans to show for this user

// --- File names from bank_system_with_care.c ---
const char *USER_FILE = "users.txt";            // Stores user login/balance data (from original UI)
const char *FILE_NAME = "accounts.csv";         // Stores full account details (authoritative source)
const char *TRANSACTION_FILE = "transactions.csv"; // Main transaction log (for admin/staff)
const char *COMPLAINT_FILE = "complaints.csv";    // Stores all customer complaints
const char *RATING_FILE = "ratings.csv";        // Stores all customer ratings
// --- FIX 1: Changed "loan.txt" to "temp_loan.txt" to match the admin panel ---
const char *LOAN_FILE = "temp_loan.txt";        // Stores all loan applications
const char *FD_FILE = "fd.txt";                 // Stores fixed deposit records

// --- Screens for this application ---
typedef enum {
    // Main Screens
    USER_DASHBOARD,     // The main menu/dashboard
    // USER_VERIFY_PIN, // Gatekeeper screen (REMOVED in V3)
    USER_TRANSFER,      // Money transfer screen
    USER_STATEMENT,     // View transaction history
    USER_CARD,          // Manage card (block/unblock)
    USER_PROFILE,       // View account details
    USER_CHANGE_PIN,    // Change login PIN
    USER_LOAN_MENU,     // Sub-menu for loans
    USER_CARE_MENU,     // Sub-menu for customer care
    USER_SUPPORT_MENU,  // Sub-menu for help/FAQs

    // Loan Sub-screens
    USER_LOAN_APPLY,    // Apply for a new loan
    USER_LOAN_DETAILS,  // View status of my loans
    USER_LOAN_EMI,      // EMI calculator
    USER_LOAN_FD,       // Fixed Deposit calculator

    // Care Sub-screens
    USER_CARE_REGISTER, // File a new complaint
    USER_CARE_VIEW,     // View my past complaints
    USER_CARE_UPDATE,   // Update a pending complaint
    USER_CARE_RATE,     // Rate the bank's service

    // Support Sub-screens
    USER_FAQS,          // View FAQs
    USER_SUPPORT        // View contact info
} AppScreen;

// Account data structure for users.txt (login file)
typedef struct {
    char username[MAX_INPUT_CHARS + 1]; // This is the Account Number
    char password[MAX_INPUT_CHARS + 1]; // This is the PIN
    double balance;                     // Current balance
    bool isCardBlocked;                 // Block status (true/false)
} Account;

// Struct for accounts.csv (from staff app, used for authoritative cross-check)
typedef struct {
    int accountNumber;      // Unique account number
    char name[100];         // Customer's full name
    char pin[10];           // Customer's PIN
    double balance;         // Current balance
    int cardBlocked;        // Block status (1 for blocked, 0 for active)
    char cardNumber[24];    // Customer's card number
    char createdBy[50];     // Staff ID of the creator
} StaffAccount;

// Transaction data structure (for simple display in statement)
// NOTE: This app uses a user-specific file like "1001_transactions.txt"
typedef struct {
    char type[20];
    double amount;
    char description[100];
} Transaction;

// --- Structs from bank_system_with_care.c ---
// Struct for temp_loan.txt
struct Loan {
    int acc_no, loan_id, tenure, type; // Loan details
    char status[20];                  // "Pending", "Approved", or "Rejected"
    float amount, rate, emi;          // Loan financial details
};

// Struct for fd.txt
struct fd {
    int n , years ;         // n = compounding periods per year
    float Principal , Rate , A ; // A = Final Amount
};

// Struct for complaints.csv
typedef struct {
    int complaintId;        // Unique ID for the complaint
    int accountNumber;      // Account that filed the complaint
    char category[50];      // Category of complaint
    char description[500];  // The user's complaint text
    char status[20];        // "Pending", "Resolved"
    char response[500];     // The staff's response text
    char timestamp[64];     // Date and time filed
} Complaint;

// Struct for ratings.csv
typedef struct {
    int accountNumber;
    int rating;             // 1-5
    char feedback[300];
    char timestamp[64];
} Rating;
// --- End of Structs ---

// Textbox helper struct
typedef struct {
    Rectangle bounds;                   // Position and size
    char text[MAX_INPUT_LARGE + 1];     // Text content (sized for large complaints)
    int charCount;                      // Current number of characters
    bool active;                        // Is the box selected?
    bool isPassword;                    // Mask text with '*'?
    int maxChars;                       // Added max chars limit
} TextBox;

//----------------------------------------------------------------------------------
// Global Variables
//----------------------------------------------------------------------------------
static AppScreen currentScreen = USER_DASHBOARD;    // Current active screen
// static AppScreen screenAfterPin = USER_DASHBOARD; // (REMOVED in V3)
static Vector2 mousePos = { 0.0f, 0.0f };       // Mouse position
static char message[300] = { 0 };               // General purpose message display (for errors)
static char resultText[500] = { 0 };            // For EMI/FD results (for success)
static bool isAccountFrozen = false;            // Flag set at startup if account is blocked
static Account currentAccount;                  // Holds all data for the logged-in user (from users.txt)
static Transaction statement[MAX_TRANSACTIONS]; // In-memory array for transactions
static int statementCount = 0;                  // Number of transactions in the array

// Arrays to hold data for viewing
static Complaint myComplaints[MAX_COMPLAINTS];  // In-memory array for user's complaints
static int myComplaintsCount = 0;               // Number of complaints in array
static struct Loan myLoans[MAX_LOANS];          // In-memory array for user's loans
static int myLoansCount = 0;                    // Number of loans in array
static int scrollOffsetY = 0;                   // Y-offset for scrolling lists

// --- Loan Application State ---
static int selectedLoanType = 0; // 1-6, 0=none
static const char *loanTypeNames[] = { "Home", "Car", "Gold", "Personal", "Business", "Education" };

// --- Complaint Application State (V7 ADDED) ---
static int selectedComplaintType = 0; // 1-6, 0=none
static const char *complaintTypeNames[] = { "Card Issues", "Transaction Problems", "Account Access", "Loan Issues", "Staff Behavior", "Other" };

// --- Transfer State ---
static bool showTransferConfirm = false; // V8 ADDED: Flag for 2-step transfer confirmation

// --- TextBoxes ---
// static TextBox pinVerifyBox; (REMOVED)
static TextBox transferUserBox;     // Recipient account number
static TextBox transferNameBox;     // V8 ADDED: Recipient name
static TextBox transferAmountBox;   // Amount to transfer
static TextBox oldPinBox;           // V3 ADDED: For "Change PIN"
static TextBox pinBox;              // For "Change PIN" -> New PIN
static TextBox confirmPinBox;       // For "Change PIN" -> Confirm New PIN
// Loan
// static TextBox loanIdBox; // REMOVED
// static TextBox loanTypeBox; // REMOVED
static TextBox loanAmountBox;       // For "Apply Loan"
static TextBox loanTenureBox;       // For "Apply Loan"
static TextBox emiAmountBox;        // For "EMI Calc"
static TextBox emiRateBox;          // For "EMI Calc"
static TextBox emiTenureBox;        // For "EMI Calc"
static TextBox fdAmountBox;         // For "FD Calc"
static TextBox fdTenureBox;         // For "FD Calc"
static TextBox fdCompoundBox;       // For "FD Calc"
// Care
// static TextBox complaintCategoryBox; // REMOVED (V7)
static TextBox complaintDescBox;    // For "Register Complaint"
static TextBox complaintIdBox;      // For "Update Complaint"
static TextBox complaintNewDescBox; // For "Update Complaint"
static TextBox ratingBox;           // For "Rate Service"
static TextBox ratingFeedbackBox;   // For "Rate Service"

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// --- GUI Helpers ---
static void InitTextBox(TextBox *box, Rectangle bounds, bool isPassword, int max); // Initializes/resets a text box
static bool DrawButton(Rectangle bounds, const char *text);               // Draws a standard button
static bool DrawSelectButton(Rectangle bounds, const char *text, bool selected); // Draws a selectable button (for loan/complaint types)
static void DrawTextBox(TextBox *box, const char *placeholder);           // Draws a text box
static void UpdateTextBox(TextBox *box);                                  // Handles keyboard input for a text box
static void ClearAllTextBoxes(void);                                      // Resets all text boxes
static void DrawBackButton(float y, AppScreen returnsTo);                 // Draws the "Back" button
static void DrawScreenTitle(const char *title);                           // Draws a centered screen title
static Rectangle GetStdButtonBounds(int position);                        // Gets a standard centered button position

// --- Functions for users.txt ---
static bool LoadAccountByUsername(char* username);            // Loads `currentAccount` from users.txt at startup
static bool UpdateAccountInFile(Account updatedAccount);      // Rewrites users.txt with updated data for one account
static bool CheckUserExists(char* username);                  // Checks if an account number exists in users.txt
static bool AddTransactionToFile(const char* username, const char* type, double amount, const char* description); // Adds to "XXXX_transactions.txt"
static int LoadTransactions(const char* username);            // Loads from "XXXX_transactions.txt" into `statement` array
static bool PerformTransfer(char* recipient, double amount);  // Core transfer logic

// --- Functions to sync with accounts.csv ---
static StaffAccount getStaffAccount(int accNo); // Reads a single account's details from accounts.csv
static void updateStaffAccount(StaffAccount acc); // Rewrites a single account's details in accounts.csv

// --- Ported functions from bank_system_with_care.c ---
// Helpers
static void getCurrentTimestamp(char *buffer, size_t size); // Gets current date/time string
static int getNextComplaintId(void);                        // Finds next complaint ID from complaints.csv
static int getNextLoanId(void);                             // Finds next loan ID from temp_loan.txt
static int accountExists(int accNo);                        // Helper (checks accounts.csv)
// GUI Logic
static void GuiChangePin(void);         // Logic for "Change PIN" button
static void GuiApplyLoan(void);         // Logic for "Apply Loan" button
static void GuiLoanDetails(void);       // Loads data into `myLoans` array
static void GuiEmiCalc(void);           // Logic for "EMI Calc" button
static void GuiFixDeposit(void);        // Logic for "FD Calc" button
static void GuiRegisterComplaint(void); // Logic for "Register Complaint" button
static void GuiViewMyComplaints(void);  // Loads data into `myComplaints` array
static void GuiUpdateComplaint(void);   // Logic for "Update Complaint" button
static void GuiRateService(void);       // Logic for "Rate Service" button

//----------------------------------------------------------------------------------
// Main Entry Point
//----------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    // --- CRITICAL: Get username (account number) from command-line ---
    if (argc < 2)
    {
        printf("Error: No account number provided. Launch this from the login app.\n");
        return 1; // Exit if not launched correctly
    }

    // Load the account data for the user who just logged in (from users.txt)
    if (!LoadAccountByUsername(argv[1]))
    {
        printf("Error: Could not load account for user '%s' from users.txt.\n", argv[1]);
        return 1; // Exit if account not in users.txt
    }
    // Success! `currentAccount` is now loaded.

    // --- V5 FIX: AUTHORITATIVE BLOCK CHECK ---
    // Cross-check the authoritative accounts.csv file
    StaffAccount staffAcc = getStaffAccount(atoi(currentAccount.username));
    if (staffAcc.accountNumber == 0)
    {
        // Account exists in users.txt but not accounts.csv (bad data state)
        printf("Error: Account data mismatch. Please contact support.\n");
        return 1;
    }
    
    // --- MODIFIED: Handle frozen account ---
    if (staffAcc.cardBlocked == 1) // If accounts.csv says blocked
    {
        isAccountFrozen = true; // Set the global frozen flag
        // We must also update users.txt to match, so login.exe catches it next time
        if (!currentAccount.isCardBlocked)
        {
            currentAccount.isCardBlocked = true;
            UpdateAccountInFile(currentAccount); // Sync users.txt
        }
        // DO NOT return. Let the window open to show the "FROZEN" message.
    }
    // --- END MODIFICATION ---
    
    // If admin un-blocked the card, but users.txt is stale, sync it.
    if (staffAcc.cardBlocked == 0 && currentAccount.isCardBlocked)
    {
        currentAccount.isCardBlocked = false;
        UpdateAccountInFile(currentAccount); // Sync users.txt
    }
    // --- END V5 FIX ---


    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "User Dashboard"); // Create the window
    SetWindowState(FLAG_WINDOW_RESIZABLE); // Allow resizing

    // Standard GUI element dimensions
    float boxWidth = 500;
    float boxHeight = 50;
    float spacing = 20;
    float startX = (SCREEN_WIDTH - boxWidth) / 2; // Center X
    float startY = 300; // Standard start Y for forms

    // --- Initialize all text boxes ---
    // InitTextBox(&pinVerifyBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, true, 4); // (REMOVED)
    // --- V8: Reordered Transfer Textboxes ---
    InitTextBox(&transferUserBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false, MAX_INPUT_CHARS);
    InitTextBox(&transferNameBox, (Rectangle){ startX, startY + boxHeight + spacing, boxWidth, boxHeight }, false, MAX_INPUT_NAME);
    InitTextBox(&transferAmountBox, (Rectangle){ startX, startY + (boxHeight + spacing)*2, boxWidth, boxHeight }, false, MAX_INPUT_CHARS);
    // ---
    InitTextBox(&oldPinBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, true, 4); // ADDED
    InitTextBox(&pinBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, true, 4);
    InitTextBox(&confirmPinBox, (Rectangle){ startX, startY + boxHeight + spacing, boxWidth, boxHeight }, true, 4);
    // Loan
    // InitTextBox(&loanIdBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false, 10); // REMOVED
    // InitTextBox(&loanTypeBox, (Rectangle){ startX, startY + (boxHeight + spacing), boxWidth, boxHeight }, false, 2); // REMOVED
    InitTextBox(&loanAmountBox, (Rectangle){ startX, startY + (boxHeight + spacing)*2, boxWidth, boxHeight }, false, 10);
    InitTextBox(&loanTenureBox, (Rectangle){ startX, startY + (boxHeight + spacing)*3, boxWidth, boxHeight }, false, 3);
    InitTextBox(&emiAmountBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false, 10);
    InitTextBox(&emiRateBox, (Rectangle){ startX, startY + (boxHeight + spacing), boxWidth, boxHeight }, false, 5);
    InitTextBox(&emiTenureBox, (Rectangle){ startX, startY + (boxHeight + spacing)*2, boxWidth, boxHeight }, false, 3);
    InitTextBox(&fdAmountBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false, 10);
    InitTextBox(&fdTenureBox, (Rectangle){ startX, startY + (boxHeight + spacing), boxWidth, boxHeight }, false, 2);
    InitTextBox(&fdCompoundBox, (Rectangle){ startX, startY + (boxHeight + spacing)*2, boxWidth, boxHeight }, false, 2);
    // Care
    // InitTextBox(&complaintCategoryBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false, 2); // REMOVED (V7)
    InitTextBox(&complaintDescBox, (Rectangle){ startX, startY + (boxHeight + spacing), boxWidth, boxHeight * 3 }, false, MAX_INPUT_LARGE);
    InitTextBox(&complaintIdBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false, 10);
    InitTextBox(&complaintNewDescBox, (Rectangle){ startX, startY + (boxHeight + spacing), boxWidth, boxHeight * 3 }, false, MAX_INPUT_LARGE);
    InitTextBox(&ratingBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false, 1);
    InitTextBox(&ratingFeedbackBox, (Rectangle){ startX, startY + (boxHeight + spacing), boxWidth, boxHeight * 3 }, false, 299);

    SetTargetFPS(60); // Set target FPS
    //--------------------------------------------------------------------------------------

    // Main loop
    while (!WindowShouldClose()) // Loop until window closed
    {
        // Update
        //----------------------------------------------------------------------------------
        mousePos = GetMousePosition(); // Get mouse position every frame

        // --- Global Textbox Activation ---
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) // On click
        {
            // Deactivate all boxes
            // pinVerifyBox.active = false; (REMOVED)
            transferUserBox.active = false;
            transferNameBox.active = false; // ADDED
            transferAmountBox.active = false;
            oldPinBox.active = false; // ADDED
            pinBox.active = false;
            confirmPinBox.active = false;
            // loanIdBox.active = false; // REMOVED
            // loanTypeBox.active = false; // REMOVED
            loanAmountBox.active = false;
            loanTenureBox.active = false;
            emiAmountBox.active = false;
            emiRateBox.active = false;
            emiTenureBox.active = false;
            fdAmountBox.active = false;
            fdTenureBox.active = false;
            fdCompoundBox.active = false;
            // complaintCategoryBox.active = false; // REMOVED (V7)
            complaintDescBox.active = false;
            complaintIdBox.active = false;
            complaintNewDescBox.active = false;
            ratingBox.active = false;
            ratingFeedbackBox.active = false;

            // Activate the one clicked (if any) based on the current screen
            switch (currentScreen)
            {
                /* (REMOVED)
                case USER_VERIFY_PIN:
                    if (CheckCollisionPointRec(mousePos, pinVerifyBox.bounds)) pinVerifyBox.active = true;
                    break;
                */
                case USER_TRANSFER:
                    if (CheckCollisionPointRec(mousePos, transferUserBox.bounds)) transferUserBox.active = true;
                    if (CheckCollisionPointRec(mousePos, transferNameBox.bounds)) transferNameBox.active = true; // ADDED
                    if (CheckCollisionPointRec(mousePos, transferAmountBox.bounds)) transferAmountBox.active = true;
                    break;
                case USER_CHANGE_PIN:
                    if (CheckCollisionPointRec(mousePos, oldPinBox.bounds)) oldPinBox.active = true; // ADDED
                    if (CheckCollisionPointRec(mousePos, pinBox.bounds)) pinBox.active = true;
                    if (CheckCollisionPointRec(mousePos, confirmPinBox.bounds)) confirmPinBox.active = true;
                    break;
                case USER_LOAN_APPLY:
                    // if (CheckCollisionPointRec(mousePos, loanIdBox.bounds)) loanIdBox.active = true; // REMOVED
                    // if (CheckCollisionPointRec(mousePos, loanTypeBox.bounds)) loanTypeBox.active = true; // REMOVED
                    if (CheckCollisionPointRec(mousePos, loanAmountBox.bounds)) loanAmountBox.active = true;
                    if (CheckCollisionPointRec(mousePos, loanTenureBox.bounds)) loanTenureBox.active = true;
                    break;
                case USER_LOAN_EMI:
                    if (CheckCollisionPointRec(mousePos, emiAmountBox.bounds)) emiAmountBox.active = true;
                    if (CheckCollisionPointRec(mousePos, emiRateBox.bounds)) emiRateBox.active = true;
                    if (CheckCollisionPointRec(mousePos, emiTenureBox.bounds)) emiTenureBox.active = true;
                    break;
                case USER_LOAN_FD:
                    if (CheckCollisionPointRec(mousePos, fdAmountBox.bounds)) fdAmountBox.active = true;
                    if (CheckCollisionPointRec(mousePos, fdTenureBox.bounds)) fdTenureBox.active = true;
                    if (CheckCollisionPointRec(mousePos, fdCompoundBox.bounds)) fdCompoundBox.active = true;
                    break;
                case USER_CARE_REGISTER:
                    // if (CheckCollisionPointRec(mousePos, complaintCategoryBox.bounds)) complaintCategoryBox.active = true; // REMOVED (V7)
                    if (CheckCollisionPointRec(mousePos, complaintDescBox.bounds)) complaintDescBox.active = true;
                    break;
                case USER_CARE_UPDATE:
                    if (CheckCollisionPointRec(mousePos, complaintIdBox.bounds)) complaintIdBox.active = true;
                    if (CheckCollisionPointRec(mousePos, complaintNewDescBox.bounds)) complaintNewDescBox.active = true;
                    break;
                case USER_CARE_RATE:
                    if (CheckCollisionPointRec(mousePos, ratingBox.bounds)) ratingBox.active = true;
                    if (CheckCollisionPointRec(mousePos, ratingFeedbackBox.bounds)) ratingFeedbackBox.active = true;
                    break;
                default: break; // No text boxes on other screens
            }
        }

        // --- Global Textbox Update ---
        // Update the content of whichever text box is active
        // if (pinVerifyBox.active) UpdateTextBox(&pinVerifyBox); (REMOVED)
        if (transferUserBox.active) UpdateTextBox(&transferUserBox);
        if (transferNameBox.active) UpdateTextBox(&transferNameBox); // ADDED
        if (transferAmountBox.active) UpdateTextBox(&transferAmountBox);
        if (oldPinBox.active) UpdateTextBox(&oldPinBox); // ADDED
        if (pinBox.active) UpdateTextBox(&pinBox);
        if (confirmPinBox.active) UpdateTextBox(&confirmPinBox);
        // if (loanIdBox.active) UpdateTextBox(&loanIdBox); // REMOVED
        // if (loanTypeBox.active) UpdateTextBox(&loanTypeBox); // REMOVED
        if (loanAmountBox.active) UpdateTextBox(&loanAmountBox);
        if (loanTenureBox.active) UpdateTextBox(&loanTenureBox);
        if (emiAmountBox.active) UpdateTextBox(&emiAmountBox);
        if (emiRateBox.active) UpdateTextBox(&emiRateBox);
        if (emiTenureBox.active) UpdateTextBox(&emiTenureBox);
        if (fdAmountBox.active) UpdateTextBox(&fdAmountBox);
        if (fdTenureBox.active) UpdateTextBox(&fdTenureBox);
        if (fdCompoundBox.active) UpdateTextBox(&fdCompoundBox);
        // if (complaintCategoryBox.active) UpdateTextBox(&complaintCategoryBox); // REMOVED (V7)
        if (complaintDescBox.active) UpdateTextBox(&complaintDescBox);
        if (complaintIdBox.active) UpdateTextBox(&complaintIdBox);
        if (complaintNewDescBox.active) UpdateTextBox(&complaintNewDescBox);
        if (ratingBox.active) UpdateTextBox(&ratingBox);
        if (ratingFeedbackBox.active) UpdateTextBox(&ratingFeedbackBox);

        // --- Scrolling for lists ---
        if (currentScreen == USER_STATEMENT || currentScreen == USER_CARE_VIEW || currentScreen == USER_LOAN_DETAILS)
        {
            scrollOffsetY += (int)GetMouseWheelMove() * 20; // Move 20 pixels per scroll
            if (scrollOffsetY > 0) scrollOffsetY = 0; // Don't scroll past the top
            // Max scroll limit is calculated in the draw loop
        }
        else
        {
            scrollOffsetY = 0; // Reset scroll on all other screens
        }
        
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing(); // Start drawing phase
            ClearBackground((Color){ 245, 245, 245, 255 }); // Light gray background

            // Draw header bar
            DrawRectangle(0, 0, GetScreenWidth(), 80, (Color){ 60, 90, 150, 255 }); // Dark blue bar
            DrawText(TextFormat("DAIICT Swiss Bank: Account #%s", currentAccount.username), 30, 20, 40, WHITE); // Title
            
            // --- NEW FROZEN ACCOUNT CHECK ---
            if (isAccountFrozen) // If this flag was set at startup
            {
                // Draw a big red message and a logout button
                const char *freezeMsg = "Your Account is FROZEN. Please contact admin.";
                int textWidth = MeasureText(freezeMsg, 40);
                DrawText(freezeMsg, (GetScreenWidth() - textWidth) / 2, GetScreenHeight() / 2 - 40, 40, RED);
                
                Rectangle logoutBtn = { (GetScreenWidth() - 400) / 2, GetScreenHeight() / 2 + 40, 400, 60 };
                if (DrawButton(logoutBtn, "Logout"))
                {
                    system("start login.exe"); // Relaunch login app
                    CloseWindow(); // Close this app
                    return 0; // Exit main
                }
                EndDrawing(); // Stop drawing here
                continue; // Skip the rest of the draw loop
            }
            // --- END NEW FROZEN ACCOUNT CHECK ---
            
            // Re-calculate standard positions based on current screen size (for responsiveness)
            startX = (GetScreenWidth() - boxWidth) / 2;
            startY = 300;
            
            // --- V5 FIX: Draw messages at bottom-center ---
            if (message[0] != '\0') // If there is an error message
            {
                int textWidth = MeasureText(message, 20);
                DrawText(message, (GetScreenWidth() - textWidth) / 2, GetScreenHeight() - 60, 20, (Color){ 220, 50, 50, 255 }); // Draw in red
            }
            if (resultText[0] != '\0') // If there is a success message
            {
                int textWidth = MeasureText(resultText, 20);
                DrawText(resultText, (GetScreenWidth() - textWidth) / 2, GetScreenHeight() - 35, 20, (Color){ 0, 150, 0, 255 }); // Draw in green
            }
            // --- END V5 FIX ---


            switch (currentScreen) // Draw content based on current screen
            {
                case USER_DASHBOARD: // Main menu screen
                {
                    float dashStartX = 150;
                    float dashWidth = GetScreenWidth() - 300;
                    float btnWidth = (dashWidth / 2) - 10;
                    float btnHeight = 70;
                    float dashStartY = 240; // Start buttons lower
                    
                    // --- V6 FIX: Refresh account data from file to see external (admin/staff) changes ---
                    LoadAccountByUsername(currentAccount.username); 
                    
                    // Get full name from accounts.csv
                    StaffAccount staffAcc = getStaffAccount(atoi(currentAccount.username));
                    char welcomeText[100];
                    if (staffAcc.accountNumber != 0) {
                        sprintf(welcomeText, "Welcome, %s!", staffAcc.name); // Welcome by name
                    } else {
                        sprintf(welcomeText, "Welcome, User %s!", currentAccount.username); // Fallback
                    }
                    
                    DrawText(welcomeText, dashStartX, dashStartY - 150, 40, (Color){ 50, 50, 50, 255 }); // Draw welcome
                    
                    // --- Balance Box ---
                    DrawRectangle(dashStartX, dashStartY - 90, dashWidth, 80, WHITE); // Background
                    DrawRectangleLines(dashStartX, dashStartY - 90, dashWidth, 80, LIGHTGRAY); // Border
                    DrawText("Current Balance", dashStartX + 30, dashStartY - 70, 20, GRAY);
                    DrawText(TextFormat("Rs %.2f", currentAccount.balance), dashStartX + 30, dashStartY - 45, 40, (Color){ 0, 100, 0, 255 }); // Balance in green
                    // Card status (color-coded)
                    DrawText(TextFormat("Card: %s", currentAccount.isCardBlocked ? "BLOCKED" : "ACTIVE"), dashStartX + dashWidth - 250, dashStartY - 55, 30,
                        currentAccount.isCardBlocked ? (Color){ 220, 50, 50, 255 } : (Color){ 0, 150, 0, 255 });


                    // --- New Button Layout ---
                    Rectangle btnTransfer = { dashStartX, dashStartY, btnWidth, btnHeight };
                    Rectangle btnStatement = { dashStartX + btnWidth + 20, dashStartY, btnWidth, btnHeight };
                    
                    Rectangle btnCard = { dashStartX, dashStartY + btnHeight + spacing, btnWidth, btnHeight };
                    Rectangle btnProfile = { dashStartX + btnWidth + 20, dashStartY + btnHeight + spacing, btnWidth, btnHeight };
                    
                    Rectangle btnChangePin = { dashStartX, dashStartY + (btnHeight + spacing)*2, btnWidth, btnHeight };
                    Rectangle btnLoanMenu = { dashStartX + btnWidth + 20, dashStartY + (btnHeight + spacing)*2, btnWidth, btnHeight };

                    Rectangle btnCareMenu = { dashStartX, dashStartY + (btnHeight + spacing)*3, btnWidth, btnHeight };
                    Rectangle btnSupportMenu = { dashStartX + btnWidth + 20, dashStartY + (btnHeight + spacing)*3, btnWidth, btnHeight };
                    
                    Rectangle btnLogout = { dashStartX, dashStartY + (btnHeight + spacing)*4 + 10, dashWidth, 50 }; // Logout button

                    // --- Draw Buttons and Handle Clicks ---
                    // Row 1
                    if (DrawButton(btnTransfer, "Transfer Money"))
                    {
                        // screenAfterPin = USER_TRANSFER; // (REMOVED V3)
                        currentScreen = USER_TRANSFER; // MODIFIED: Go directly
                        // strcpy(message, "Please enter PIN to transfer money."); // (REMOVED V3)
                    }
                    if (DrawButton(btnStatement, "View Statement"))
                    {
                        statementCount = LoadTransactions(currentAccount.username); // Load data
                        // screenAfterPin = USER_STATEMENT; // (REMOVED V3)
                        currentScreen = USER_STATEMENT; // MODIFIED: Go directly
                        // strcpy(message, "Please enter PIN to view statement."); // (REMOVED V3)
                    }
                    // Row 2
                    if (DrawButton(btnCard, "Manage Card"))
                    {
                        // screenAfterPin = USER_CARD; // (REMOVED V3)
                        currentScreen = USER_CARD; // MODIFIED: Go directly
                        // strcpy(message, "Please enter PIN to manage your card."); // (REMOVED V3)
                    }
                    if (DrawButton(btnProfile, "View Profile"))
                    {
                        // screenAfterPin = USER_PROFILE; // (REMOVED V3)
                        currentScreen = USER_PROFILE; // MODIFIED: Go directly
                        // strcpy(message, "Please enter PIN to view your profile."); // (REMOVED V3)
                    }
                    // Row 3
                    if (DrawButton(btnChangePin, "Change PIN"))
                    {
                        // screenAfterPin = USER_CHANGE_PIN; // (REMOVED V3)
                        currentScreen = USER_CHANGE_PIN; // MODIFIED: Go directly
                        strcpy(message, ""); // Clear message
                    }
                    if (DrawButton(btnLoanMenu, "Loan Menu"))
                    {
                        // screenAfterPin = USER_LOAN_MENU; // (REMOVED V3)
                        currentScreen = USER_LOAN_MENU; // MODIFIED: Go directly
                        // strcpy(message, "Please enter PIN to access loan services."); // (REMOVED V3)
                    }
                    // Row 4
                    if (DrawButton(btnCareMenu, "Customer Care"))
                    {
                        // screenAfterPin = USER_CARE_MENU; // (REMOVED V3)
                        currentScreen = USER_CARE_MENU; // MODIFIED: Go directly
                        // strcpy(message, "Please enter PIN to access customer care."); // (REMOVED V3)
                    }
                    if (DrawButton(btnSupportMenu, "FAQs & Support"))
                    {
                        currentScreen = USER_SUPPORT_MENU; // No PIN needed
                    }
                    // Row 5
                    if (DrawButton(btnLogout, "Logout"))
                    {
                        system("start login.exe"); // Relaunch login app
                        CloseWindow(); // Close this app
                        return 0; // Exit main
                    }
                    
                    // V5 FIX: Global message draw at bottom handles this
                    // DrawText(message, dashStartX, GetScreenHeight() - 60, 20, (Color){ 50, 150, 50, 255 });
                    
                } break;
                
                /* (REMOVED V3)
                case USER_VERIFY_PIN:
                {
                    // ...
                } break;
                */

                case USER_TRANSFER: // Money Transfer screen
                {
                    DrawScreenTitle("Transfer Money"); // Title
                    
                    // V8: Adjust layout for 3 boxes
                    transferUserBox.bounds.x = startX;
                    transferNameBox.bounds.x = startX;
                    transferAmountBox.bounds.x = startX;
                    transferUserBox.bounds.y = startY;
                    transferNameBox.bounds.y = startY + boxHeight + spacing;
                    transferAmountBox.bounds.y = startY + (boxHeight + spacing)*2;
                    
                    // --- V8: Transfer Confirmation Logic ---
                    if (!showTransferConfirm) // Step 1: Get details
                    {
                        DrawTextBox(&transferUserBox, "Recipient Account Number");
                        DrawTextBox(&transferNameBox, "Recipient Full Name (Case Sensitive)");
                        DrawTextBox(&transferAmountBox, "Amount (e.g., 50.00)");

                        Rectangle actionButton = GetStdButtonBounds(3); // Button after 3rd box
                        
                        if (DrawButton(actionButton, "Confirm Transfer"))
                        {
                            strcpy(message, ""); // Clear messages
                            strcpy(resultText, "");
                            double amount = atof(transferAmountBox.text); // Get amount
                            
                            // --- Validation ---
                            if (strcmp(transferUserBox.text, currentAccount.username) == 0)
                            {
                                strcpy(message, "Error: Cannot transfer to yourself.");
                            }
                            else if (!CheckUserExists(transferUserBox.text))
                            {
                                strcpy(message, "Error: Recipient account does not exist.");
                            }
                            else if (strlen(transferNameBox.text) == 0)
                            {
                                strcpy(message, "Error: Please enter recipient's name.");
                            }
                            else if (amount <= 0)
                            {
                                strcpy(message, "Error: Amount must be positive.");
                            }
                            else if (amount > currentAccount.balance)
                            {
                                strcpy(message, "Error: Insufficient funds.");
                            }
                            else
                            {
                                // --- V8 USER REQUEST: Verify Name ---
                                // Get recipient's full details from accounts.csv
                                StaffAccount recipientStaffAcc = getStaffAccount(atoi(transferUserBox.text));
                                if (recipientStaffAcc.accountNumber == 0) {
                                    strcpy(message, "Error: Recipient account details not found.");
                                } else if (strcmp(transferNameBox.text, recipientStaffAcc.name) != 0) {
                                    // Check if entered name matches the name in the file
                                    strcpy(message, "Error: Details mismatched. Account name and number do not match.");
                                } else {
                                    // All checks passed, show confirmation screen
                                    showTransferConfirm = true;
                                    sprintf(message, "Transfer Rs %.2f to %s (Acct: %s)?", 
                                            amount, recipientStaffAcc.name, transferUserBox.text);
                                }
                                // --- END USER REQUEST ---
                            }
                        }
                    }
                    else // Step 2: Show confirmation
                    {
                        // Show confirmation message (which we set in `message`)
                        int textWidth = MeasureText(message, 20);
                        DrawText(message, (GetScreenWidth() - textWidth) / 2, startY + 20, 20, (Color){ 220, 50, 50, 255 }); // Draw message in red

                        Rectangle yesButton = { startX, startY + 60, (boxWidth / 2) - 10, 50 };
                        Rectangle noButton = { startX + (boxWidth / 2) + 10, startY + 60, (boxWidth / 2) - 10, 50 };

                        if (DrawButton(yesButton, "YES, Transfer"))
                        {
                            double amount = atof(transferAmountBox.text);
                            if (PerformTransfer(transferUserBox.text, amount)) // Call core logic
                            {
                                strcpy(message, "Transfer Successful!"); // Set success
                                currentScreen = USER_DASHBOARD; // Go back to dashboard
                            }
                            else
                            {
                                strcpy(message, "Error: Transfer failed. File error."); // Set error
                            }
                            ClearAllTextBoxes();
                            showTransferConfirm = false; // Reset state
                        }
                        
                        if (DrawButton(noButton, "NO, Cancel"))
                        {
                            showTransferConfirm = false; // Go back to Step 1
                            strcpy(message, "Transfer cancelled.");
                            // Keep textboxes filled for correction
                        }
                    }
                    // --- END V8 LOGIC ---
                    
                    DrawBackButton(GetScreenHeight() - 70, USER_DASHBOARD); // Back button
                } break;

                case USER_STATEMENT: // View Transaction Statement
                {
                    float listStartX = 150;
                    float listWidth = GetScreenWidth() - 300;
                    float itemHeight = 40; // Height of each transaction line
                    
                    DrawScreenTitle("Account Statement"); // Title
                    
                    if (statementCount == 0) // If no transactions
                    {
                        DrawText("No transactions found.", listStartX, startY, 20, GRAY);
                    }
                    else
                    {
                        // --- Draw Header ---
                        DrawRectangle(listStartX, startY - 50, listWidth, itemHeight, (Color){ 220, 220, 220, 255 }); // Header bg
                        DrawText("Type", listStartX + 20, startY - 40, 20, BLACK);
                        DrawText("Amount", listStartX + 200, startY - 40, 20, BLACK);
                        DrawText("Description", listStartX + 400, startY - 40, 20, BLACK);
                        
                        // --- Draw List with Scrolling ---
                        // Calculate max scroll (don't let blank space scroll past bottom)
                        int maxScroll = (statementCount * itemHeight) - (GetScreenHeight() - startY - 100);
                        if (maxScroll < 0) maxScroll = 0;
                        if (scrollOffsetY < -maxScroll) scrollOffsetY = -maxScroll; // Apply max scroll limit

                        for (int i = 0; i < statementCount; i++) // Loop through loaded transactions
                        {
                            float currentY = startY + (itemHeight * i) + scrollOffsetY; // Calculate Y pos
                            // Culling: If item is off-screen, skip drawing it
                            if (currentY < startY - itemHeight || currentY > GetScreenHeight() - 100) continue; 

                            // Color-code amount (green for deposit, red for withdrawal)
                            Color amountColor = (strcmp(statement[i].type, "DEPOSIT") == 0 || strcmp(statement[i].type, "TRANSFER_IN") == 0) ?
                                                (Color){ 0, 150, 0, 255 } : (Color){ 200, 0, 0, 255 };
                                                
                            DrawText(statement[i].type, listStartX + 20, currentY + 10, 20, (Color){ 80, 80, 80, 255 });
                            DrawText(TextFormat("Rs %.2f", statement[i].amount), listStartX + 200, currentY + 10, 20, amountColor);
                            DrawText(statement[i].description, listStartX + 400, currentY + 10, 20, (Color){ 50, 50, 50, 255 });
                            DrawLine(listStartX, currentY + itemHeight, listStartX + listWidth, currentY + itemHeight, LIGHTGRAY); // Divider
                        }
                    }
                    DrawBackButton(GetScreenHeight() - 70, USER_DASHBOARD); // Back button
                } break;

                case USER_CARD: // Manage Card (Block/Unblock)
                {
                    DrawScreenTitle("Manage Card"); // Title
                    
                    // Display current status
                    char statusText[100];
                    sprintf(statusText, "Card Status: %s", currentAccount.isCardBlocked ? "BLOCKED" : "ACTIVE");
                    Color statusColor = currentAccount.isCardBlocked ? (Color){ 220, 50, 50, 255 } : (Color){ 0, 150, 0, 255 };
                    DrawText(statusText, startX, startY + 50, 30, statusColor);
                    
                    // Toggle button text
                    const char* buttonText = currentAccount.isCardBlocked ? "Unblock Card" : "Block Card";
                    Rectangle actionButton = GetStdButtonBounds(1); // Get button position
                    
                    if (DrawButton(actionButton, buttonText)) // On click
                    {
                        strcpy(message, ""); // Clear messages
                        strcpy(resultText, "");
                        currentAccount.isCardBlocked = !currentAccount.isCardBlocked; // Toggle flag
                        if (UpdateAccountInFile(currentAccount)) // Write change to users.txt
                        {
                            // Sync card status with accounts.csv
                            int accNo = atoi(currentAccount.username);
                            StaffAccount staffAcc = getStaffAccount(accNo); // Get data
                            if (staffAcc.accountNumber != 0) {
                                staffAcc.cardBlocked = currentAccount.isCardBlocked ? 1 : 0; // Sync flag
                                updateStaffAccount(staffAcc); // Write change to accounts.csv
                            }
                            // V5 FIX: Use resultText for success
                            strcpy(resultText, "Card status updated successfully.");
                        }
                        else
                        {
                            strcpy(message, "Error: Could not update card status in users.txt.");
                            currentAccount.isCardBlocked = !currentAccount.isCardBlocked; // Revert flag on failure
                        }
                    }
                    
                    DrawBackButton(GetScreenHeight() - 70, USER_DASHBOARD); // Back button
                    // V5 FIX: Message is now drawn globally at the bottom
                    // DrawText(message, startX, actionButton.y + 70, 20, (Color){ 80, 80, 80, 255 });
                } break;

                // --- NEW SCREENS ---

                case USER_PROFILE: // View Account Profile
                {
                    DrawScreenTitle("Account Profile"); // Title
                    // Get full details from authoritative source (accounts.csv)
                    StaffAccount acc = getStaffAccount(atoi(currentAccount.username));
                    float profileY = startY - 50;
                    float lineSpacing = 40;

                    // --- FIX: Convert card number from potential e-notation ---
                    // If the CSV stores a long number like 1234567890123456 as "1.23457e+15"
                    // atof() will convert the string to a double.
                    double card_num_double = atof(acc.cardNumber); 
                    char formatted_card_num[25];
                    // sprintf() with "%.0f" will format the double back into a full
                    // string of digits without decimals or scientific notation.
                    sprintf(formatted_card_num, "%.0f", card_num_double);
                    // --- END FIX ---

                    // Draw all details
                    DrawText(TextFormat("Name: %s", acc.name), startX, profileY, 30, DARKGRAY);
                    DrawText(TextFormat("Account Number: %d", acc.accountNumber), startX, profileY + lineSpacing, 30, DARKGRAY);
                    // DrawText(TextFormat("Card Number: %s", acc.cardNumber), startX, profileY + lineSpacing*2, 30, DARKGRAY); // <-- OLD LINE
                    DrawText(TextFormat("Card Number: %s", formatted_card_num), startX, profileY + lineSpacing*2, 30, DARKGRAY); // <-- NEW LINE
                    DrawText(TextFormat("Balance: Rs %.2f", acc.balance), startX, profileY + lineSpacing*3, 30, DARKGRAY);
                    DrawText(TextFormat("Account Created By: %s", acc.createdBy), startX, profileY + lineSpacing*4, 30, DARKGRAY);
                    
                    DrawBackButton(GetScreenHeight() - 70, USER_DASHBOARD); // Back button
                } break;
                
                case USER_CHANGE_PIN: // Change PIN screen
                {
                    DrawScreenTitle("Change PIN"); // Title
                    // V3: Adjust layout for 3 boxes
                    oldPinBox.bounds.x = startX; 
                    oldPinBox.bounds.y = startY; 
                    pinBox.bounds.x = startX;
                    confirmPinBox.bounds.x = startX;
                    pinBox.bounds.y = startY + boxHeight + spacing; 
                    confirmPinBox.bounds.y = startY + (boxHeight + spacing) * 2; 
                    
                    DrawTextBox(&oldPinBox, "Enter OLD 4-Digit PIN"); // V3 ADDED
                    DrawTextBox(&pinBox, "New 4-Digit PIN");
                    DrawTextBox(&confirmPinBox, "Confirm New PIN");
                    
                    Rectangle actionButton = GetStdButtonBounds(3); // Button after 3rd box
                    
                    if (DrawButton(actionButton, "Confirm Change"))
                    {
                        strcpy(message, ""); // Clear messages
                        strcpy(resultText, "");
                        GuiChangePin(); // Call logic function
                    }
                    
                    DrawBackButton(GetScreenHeight() - 70, USER_DASHBOARD); // Back button
                    // V5 FIX: Message is now drawn globally at the bottom
                    // DrawText(message, startX, actionButton.y + 70, 20, (Color){ 220, 50, 50, 255 });
                } break;
                
                case USER_LOAN_MENU: // Loan Sub-menu
                {
                    DrawScreenTitle("Loan & Investment Menu"); // Title
                    float menuBtnWidth = 600;
                    float menuBtnHeight = 60;
                    float menuStartX = (GetScreenWidth() - menuBtnWidth) / 2; // Centered
                    float menuStartY = startY - 50;

                    // Button layout
                    Rectangle btnApply = { menuStartX, menuStartY, menuBtnWidth, menuBtnHeight };
                    Rectangle btnDetails = { menuStartX, menuStartY + (menuBtnHeight + spacing), menuBtnWidth, menuBtnHeight };
                    Rectangle btnEmi = { menuStartX, menuStartY + (menuBtnHeight + spacing)*2, menuBtnWidth, menuBtnHeight };
                    Rectangle btnFd = { menuStartX, menuStartY + (menuBtnHeight + spacing)*3, menuBtnWidth, menuBtnHeight };

                    // Button logic
                    if (DrawButton(btnApply, "Apply for Loan"))
                    {
                        currentScreen = USER_LOAN_APPLY;
                        selectedLoanType = 0; // Reset selection
                    }
                    if (DrawButton(btnDetails, "View Loan Details"))
                    {
                        GuiLoanDetails(); // Load data into `myLoans` array
                        currentScreen = USER_LOAN_DETAILS;
                    }
                    if (DrawButton(btnEmi, "EMI Calculator")) currentScreen = USER_LOAN_EMI;
                    if (DrawButton(btnFd, "Fixed Deposit")) currentScreen = USER_LOAN_FD;
                    
                    DrawBackButton(GetScreenHeight() - 70, USER_DASHBOARD); // Back button
                } break;
                
                case USER_CARE_MENU: // Customer Care Sub-menu
                {
                    DrawScreenTitle("Customer Care Menu"); // Title
                    float menuBtnWidth = 600;
                    float menuBtnHeight = 60;
                    float menuStartX = (GetScreenWidth() - menuBtnWidth) / 2; // Centered
                    float menuStartY = startY - 50;

                    // Button layout
                    Rectangle btnRegister = { menuStartX, menuStartY, menuBtnWidth, menuBtnHeight };
                    Rectangle btnView = { menuStartX, menuStartY + (menuBtnHeight + spacing), menuBtnWidth, menuBtnHeight };
                    Rectangle btnUpdate = { menuStartX, menuStartY + (menuBtnHeight + spacing)*2, menuBtnWidth, menuBtnHeight };
                    Rectangle btnRate = { menuStartX, menuStartY + (menuBtnHeight + spacing)*3, menuBtnWidth, menuBtnHeight };
                    
                    // Button logic
                    if (DrawButton(btnRegister, "Register Complaint"))
                    {
                        currentScreen = USER_CARE_REGISTER;
                        selectedComplaintType = 0; // V7 ADDED: Reset selection
                    }
                    if (DrawButton(btnView, "View My Complaints"))
                    {
                        GuiViewMyComplaints(); // Load data into `myComplaints` array
                        currentScreen = USER_CARE_VIEW;
                    }
                    if (DrawButton(btnUpdate, "Update Complaint")) currentScreen = USER_CARE_UPDATE;
                    if (DrawButton(btnRate, "Rate Our Service")) currentScreen = USER_CARE_RATE;
                    
                    DrawBackButton(GetScreenHeight() - 70, USER_DASHBOARD); // Back button
                } break;
                
                case USER_SUPPORT_MENU: // Help/Support Sub-menu
                {
                    DrawScreenTitle("Help Center"); // Title
                    float menuBtnWidth = 600;
                    float menuBtnHeight = 60;
                    float menuStartX = (GetScreenWidth() - menuBtnWidth) / 2; // Centered
                    float menuStartY = startY;

                    // Button layout
                    Rectangle btnFaq = { menuStartX, menuStartY, menuBtnWidth, menuBtnHeight };
                    Rectangle btnSupport = { menuStartX, menuStartY + (menuBtnHeight + spacing), menuBtnWidth, menuBtnHeight };

                    // Button logic
                    if (DrawButton(btnFaq, "Frequently Asked Questions (FAQs)")) currentScreen = USER_FAQS;
                    if (DrawButton(btnSupport, "Contact Support")) currentScreen = USER_SUPPORT;
                    
                    DrawBackButton(GetScreenHeight() - 70, USER_DASHBOARD); // Back button
                } break;
                
                // --- Loan Sub-screens ---
                case USER_LOAN_APPLY: // Apply for Loan screen
                {
                    DrawScreenTitle("Apply for Loan"); // Title
                    // Recalculate layout for buttons
                    float buttonWidth = (boxWidth - spacing) / 2;
                    float buttonHeight = 50;
                    
                    // Position text boxes below the button grid
                    loanAmountBox.bounds.x = startX;
                    loanTenureBox.bounds.x = startX;
                    loanAmountBox.bounds.y = startY + (buttonHeight + spacing) * 3 + 10;
                    loanTenureBox.bounds.y = startY + (buttonHeight + spacing) * 4 + 10;

                    DrawText(TextFormat("Your Loan ID will be: %d", getNextLoanId()), startX, startY - 40, 20, DARKGRAY); // Show next ID
                    DrawText("Select Loan Type:", startX, startY - 10, 20, GRAY);
                    
                    // --- Draw 6 Loan Type Buttons in a 2x3 grid ---
                    Rectangle btnHome = { startX, startY + 20, buttonWidth, buttonHeight };
                    Rectangle btnCar = { startX + buttonWidth + spacing, startY + 20, buttonWidth, buttonHeight };
                    Rectangle btnGold = { startX, startY + (buttonHeight + spacing) + 20, buttonWidth, buttonHeight };
                    Rectangle btnPersonal = { startX + buttonWidth + spacing, startY + (buttonHeight + spacing) + 20, buttonWidth, buttonHeight };
                    Rectangle btnBusiness = { startX, startY + (buttonHeight + spacing) * 2 + 20, buttonWidth, buttonHeight };
                    Rectangle btnEducation = { startX + buttonWidth + spacing, startY + (buttonHeight + spacing) * 2 + 20, buttonWidth, buttonHeight };

                    // Draw selectable buttons, update `selectedLoanType` on click
                    if (DrawSelectButton(btnHome, loanTypeNames[0], selectedLoanType == 1)) selectedLoanType = 1;
                    if (DrawSelectButton(btnCar, loanTypeNames[1], selectedLoanType == 2)) selectedLoanType = 2;
                    if (DrawSelectButton(btnGold, loanTypeNames[2], selectedLoanType == 3)) selectedLoanType = 3;
                    if (DrawSelectButton(btnPersonal, loanTypeNames[3], selectedLoanType == 4)) selectedLoanType = 4;
                    if (DrawSelectButton(btnBusiness, loanTypeNames[4], selectedLoanType == 5)) selectedLoanType = 5;
                    if (DrawSelectButton(btnEducation, loanTypeNames[5], selectedLoanType == 6)) selectedLoanType = 6;
                    // --- End Buttons ---

                    DrawTextBox(&loanAmountBox, "Enter Loan Amount");
                    DrawTextBox(&loanTenureBox, "Enter Tenure (in months)");

                    Rectangle actionButton = GetStdButtonBounds(1); // Get button position
                    if (DrawButton(actionButton, "Submit Application"))
                    {
                        strcpy(message, ""); // Clear messages
                        strcpy(resultText, "");
                        GuiApplyLoan(); // Call logic function
                    }
                    
                    DrawBackButton(GetScreenHeight() - 70, USER_LOAN_MENU); // Back button
                    // V5 FIX: Message is now drawn globally at the bottom
                } break;

                case USER_LOAN_DETAILS: // View My Loans screen
                {
                    DrawScreenTitle("My Loan Details"); // Title
                    float listStartX = 150;
                    float listWidth = GetScreenWidth() - 300;
                    float itemHeight = 220; // Taller items for loans
                    
                    // V5 FIX: Add Refresh Button
                    if (DrawButton((Rectangle){ GetScreenWidth() - 180, 120, 150, 40 }, "Refresh"))
                    {
                        GuiLoanDetails(); // Reload data from file
                    }
                    
                    if (myLoansCount == 0) // If no loans found
                    {
                        DrawText("No loan records found.", listStartX, startY, 20, GRAY);
                    }
                    else
                    {
                        // Calculate max scroll
                        int maxScroll = (myLoansCount * itemHeight) - (GetScreenHeight() - startY - 100);
                        if (maxScroll < 0) maxScroll = 0;
                        if (scrollOffsetY < -maxScroll) scrollOffsetY = -maxScroll; // Apply limit
                        
                        for (int i = 0; i < myLoansCount; i++) // Loop through loaded loans
                        {
                            float currentY = startY + (itemHeight * i) + scrollOffsetY; // Calc Y pos
                            // Culling
                            if (currentY < startY - itemHeight || currentY > GetScreenHeight() - 100) continue;

                            DrawRectangle(listStartX, currentY, listWidth, itemHeight - spacing, WHITE); // Item bg
                            DrawRectangleLines(listStartX, currentY, listWidth, itemHeight - spacing, LIGHTGRAY); // Item border
                            
                            // Convert loan type (int) to string
                            char loanType[20];
                            switch (myLoans[i].type) {
                                case 1: strcpy(loanType, "Home"); break;
                                case 2: strcpy(loanType, "Car"); break;
                                case 3: strcpy(loanType, "Gold"); break;
                                case 4: strcpy(loanType, "Personal"); break;
                                case 5: strcpy(loanType, "Business"); break;
                                case 6: strcpy(loanType, "Education"); break;
                                default: strcpy(loanType, "Unknown");
                            }
                            
                            // Color-code status
                            Color statusColor = GRAY;
                            if (strcmp(myLoans[i].status, "Approved") == 0) statusColor = GREEN;
                            else if (strcmp(myLoans[i].status, "Pending") == 0) statusColor = ORANGE;
                            else if (strcmp(myLoans[i].status, "Rejected") == 0) statusColor = RED;

                            // Draw loan details
                            DrawText(TextFormat("Loan ID: %d (%s)", myLoans[i].loan_id, loanType), listStartX + 20, currentY + 15, 30, (Color){ 60, 90, 150, 255 });
                            DrawText(TextFormat("Status: %s", myLoans[i].status), listStartX + listWidth - 250, currentY + 20, 20, statusColor);
                            DrawLine(listStartX, currentY + 60, listStartX + listWidth, currentY + 60, LIGHTGRAY); // Divider
                            DrawText(TextFormat("Amount: Rs %.2f", myLoans[i].amount), listStartX + 20, currentY + 80, 20, DARKGRAY);
                            DrawText(TextFormat("Rate: %.2f %%", myLoans[i].rate), listStartX + 20, currentY + 110, 20, DARKGRAY);
                            DrawText(TextFormat("Tenure: %d months", myLoans[i].tenure), listStartX + 20, currentY + 140, 20, DARKGRAY);
                            DrawText(TextFormat("Monthly EMI: Rs %.2f", myLoans[i].emi), listStartX + 20, currentY + 170, 20, (Color){ 0, 100, 0, 255 });
                        }
                    }
                    DrawBackButton(GetScreenHeight() - 70, USER_LOAN_MENU); // Back button
                } break;

                case USER_LOAN_EMI: // EMI Calculator
                {
                    DrawScreenTitle("EMI Calculator"); // Title
                    // Position text boxes
                    emiAmountBox.bounds.x = startX;
                    emiRateBox.bounds.x = startX;
                    emiTenureBox.bounds.x = startX;
                    emiAmountBox.bounds.y = startY;
                    emiRateBox.bounds.y = startY + (boxHeight + spacing);
                    emiTenureBox.bounds.y = startY + (boxHeight + spacing)*2;
                    
                    DrawTextBox(&emiAmountBox, "Loan Amount");
                    DrawTextBox(&emiRateBox, "Annual Interest Rate (e.g., 8.5)");
                    DrawTextBox(&emiTenureBox, "Loan Tenure (in months)");
                    
                    Rectangle actionButton = GetStdButtonBounds(3); // Button after 3rd box
                    if (DrawButton(actionButton, "Calculate EMI"))
                    {
                        strcpy(message, ""); // Clear messages
                        strcpy(resultText, "");
                        GuiEmiCalc(); // Call logic
                    }
                    
                    DrawBackButton(GetScreenHeight() - 70, USER_LOAN_MENU); // Back button
                    // V5 FIX: Message is now drawn globally at the bottom
                } break;

                case USER_LOAN_FD: // Fixed Deposit Calculator
                {
                    DrawScreenTitle("Fixed Deposit Calculator"); // Title
                    // Position text boxes
                    fdAmountBox.bounds.x = startX;
                    fdTenureBox.bounds.x = startX;
                    fdCompoundBox.bounds.x = startX;
                    fdAmountBox.bounds.y = startY;
                    fdTenureBox.bounds.y = startY + (boxHeight + spacing);
                    fdCompoundBox.bounds.y = startY + (boxHeight + spacing)*2;
                    
                    DrawTextBox(&fdAmountBox, "Principal Amount");
                    DrawTextBox(&fdTenureBox, "Tenure (in years)");
                    DrawTextBox(&fdCompoundBox, "Compounding (1=Yearly, 4=Quarterly, 12=Monthly)");
                    
                    Rectangle actionButton = GetStdButtonBounds(3); // Button after 3rd box
                    if (DrawButton(actionButton, "Calculate & Save FD"))
                    {
                        strcpy(message, ""); // Clear messages
                        strcpy(resultText, "");
                        GuiFixDeposit(); // Call logic
                    }

                    DrawBackButton(GetScreenHeight() - 70, USER_LOAN_MENU); // Back button
                    // V5 FIX: Message is now drawn globally at the bottom
                } break;
                
                // --- Care Sub-screens ---
                case USER_CARE_REGISTER: // Register Complaint screen
                {
                    DrawScreenTitle("Register Complaint"); // Title
                    complaintDescBox.bounds.x = startX;
                    
                    // --- V7 MODIFICATION: New Button Layout ---
                    float buttonWidth = (boxWidth - spacing) / 2;
                    float buttonHeight = 50;
                    // Position description box below the button grid
                    complaintDescBox.bounds.y = startY + (buttonHeight + spacing) * 3 + 10; 
                    
                    DrawText("Select Complaint Type:", startX, startY - 10, 20, GRAY);
                    
                    // --- Draw 6 Complaint Type Buttons in a 2x3 grid ---
                    Rectangle btnCat1 = { startX, startY + 20, buttonWidth, buttonHeight };
                    Rectangle btnCat2 = { startX + buttonWidth + spacing, startY + 20, buttonWidth, buttonHeight };
                    Rectangle btnCat3 = { startX, startY + (buttonHeight + spacing) + 20, buttonWidth, buttonHeight };
                    Rectangle btnCat4 = { startX + buttonWidth + spacing, startY + (buttonHeight + spacing) + 20, buttonWidth, buttonHeight };
                    Rectangle btnCat5 = { startX, startY + (buttonHeight + spacing) * 2 + 20, buttonWidth, buttonHeight };
                    Rectangle btnCat6 = { startX + buttonWidth + spacing, startY + (buttonHeight + spacing) * 2 + 20, buttonWidth, buttonHeight };

                    // Draw selectable buttons, update `selectedComplaintType` on click
                    if (DrawSelectButton(btnCat1, complaintTypeNames[0], selectedComplaintType == 1)) selectedComplaintType = 1;
                    if (DrawSelectButton(btnCat2, complaintTypeNames[1], selectedComplaintType == 2)) selectedComplaintType = 2;
                    if (DrawSelectButton(btnCat3, complaintTypeNames[2], selectedComplaintType == 3)) selectedComplaintType = 3;
                    if (DrawSelectButton(btnCat4, complaintTypeNames[3], selectedComplaintType == 4)) selectedComplaintType = 4;
                    if (DrawSelectButton(btnCat5, complaintTypeNames[4], selectedComplaintType == 5)) selectedComplaintType = 5;
                    if (DrawSelectButton(btnCat6, complaintTypeNames[5], selectedComplaintType == 6)) selectedComplaintType = 6;
                    // --- End Buttons ---
                    
                    // DrawText("1.Card, 2.Transaction, 3.Access, 4.Loan, 5.Staff, 6.Other", startX, startY - 25, 10, GRAY); // REMOVED (V7)
                    // DrawTextBox(&complaintCategoryBox, "Category (1-6)"); // REMOVED (V7)
                    DrawTextBox(&complaintDescBox, "Describe your complaint (max 500 chars)"); // Large text box
                    
                    Rectangle actionButton = GetStdButtonBounds(1); // Get button position
                    if (DrawButton(actionButton, "Submit Complaint"))
                    {
                        strcpy(message, ""); // Clear messages
                        strcpy(resultText, "");
                        GuiRegisterComplaint(); // Call logic
                    }

                    DrawBackButton(GetScreenHeight() - 70, USER_CARE_MENU); // Back button
                    // V5 FIX: Message is now drawn globally at the bottom
                } break;

                case USER_CARE_VIEW: // View My Complaints screen
                {
                    DrawScreenTitle("My Complaints"); // Title
                    float listStartX = 150;
                    float listWidth = GetScreenWidth() - 300;
                    float itemHeight = 250; // Taller items for complaints
                    
                    // V5 FIX: Add Refresh Button
                    if (DrawButton((Rectangle){ GetScreenWidth() - 180, 120, 150, 40 }, "Refresh"))
                    {
                        GuiViewMyComplaints(); // Reload data from file
                    }
                    
                    if (myComplaintsCount == 0) // If no complaints
                    {
                        DrawText("No complaints found for this account.", listStartX, startY, 20, GRAY);
                    }
                    else
                    {
                        // Calculate max scroll
                        int maxScroll = (myComplaintsCount * itemHeight) - (GetScreenHeight() - startY - 100);
                        if (maxScroll < 0) maxScroll = 0;
                        if (scrollOffsetY < -maxScroll) scrollOffsetY = -maxScroll; // Apply limit
                        
                        for (int i = 0; i < myComplaintsCount; i++) // Loop through loaded complaints
                        {
                            float currentY = startY + (itemHeight * i) + scrollOffsetY; // Calc Y pos
                            // Culling
                            if (currentY < startY - itemHeight || currentY > GetScreenHeight() - 100) continue;

                            DrawRectangle(listStartX, currentY, listWidth, itemHeight - spacing, WHITE); // Item bg
                            DrawRectangleLines(listStartX, currentY, listWidth, itemHeight - spacing, LIGHTGRAY); // Item border
                            
                            // Draw complaint details
                            DrawText(TextFormat("ID: %d | %s", myComplaints[i].complaintId, myComplaints[i].category), listStartX + 20, currentY + 15, 30, (Color){ 60, 90, 150, 255 });
                            DrawText(TextFormat("Status: %s", myComplaints[i].status), listStartX + listWidth - 200, currentY + 20, 20, (myComplaints[i].status[0] == 'R') ? GREEN : ORANGE);
                            DrawLine(listStartX, currentY + 60, listStartX + listWidth, currentY + 60, LIGHTGRAY); // Divider
                            
                            DrawText("Your Complaint:", listStartX + 20, currentY + 70, 10, GRAY);
                            DrawText(myComplaints[i].description, listStartX + 20, currentY + 85, 20, DARKGRAY);
                            
                            DrawText("Bank Response:", listStartX + 20, currentY + 150, 10, GRAY);
                            DrawText(myComplaints[i].response, listStartX + 20, currentY + 165, 20, (Color){ 0, 100, 0, 255 }); // Response in green
                            
                            DrawText(myComplaints[i].timestamp, listStartX + 20, currentY + 200, 10, GRAY); // Timestamp
                        }
                    }
                    DrawBackButton(GetScreenHeight() - 70, USER_CARE_MENU); // Back button
                } break;

                case USER_CARE_UPDATE: // Update Complaint screen
                {
                    DrawScreenTitle("Update Complaint"); // Title
                    // Position text boxes
                    complaintIdBox.bounds.x = startX;
                    complaintNewDescBox.bounds.x = startX;
                    complaintIdBox.bounds.y = startY;
                    complaintNewDescBox.bounds.y = startY + boxHeight + spacing;
                    
                    DrawTextBox(&complaintIdBox, "Complaint ID to Update");
                    DrawTextBox(&complaintNewDescBox, "Enter new description (max 500 chars)");
                    DrawText("Note: You can only update complaints that are still 'Pending'.", startX, startY + (boxHeight*4) + spacing*2, 10, GRAY);

                    Rectangle actionButton = GetStdButtonBounds(3); // Button after 3rd box
                    if (DrawButton(actionButton, "Update Complaint"))
                    {
                        strcpy(message, ""); // Clear messages
                        strcpy(resultText, "");
                        GuiUpdateComplaint(); // Call logic
                    }
                    
                    DrawBackButton(GetScreenHeight() - 70, USER_CARE_MENU); // Back button
                    // V5 FIX: Message is now drawn globally at the bottom
                } break;

                case USER_CARE_RATE: // Rate Service screen
                {
                    DrawScreenTitle("Rate Our Service"); // Title
                    // Position text boxes
                    ratingBox.bounds.x = startX;
                    ratingFeedbackBox.bounds.x = startX;
                    ratingBox.bounds.y = startY;
                    ratingFeedbackBox.bounds.y = startY + boxHeight + spacing;
                    
                    DrawTextBox(&ratingBox, "Rating (1-5 stars)");
                    DrawTextBox(&ratingFeedbackBox, "Additional feedback (optional, max 300 chars)");
                    
                    Rectangle actionButton = GetStdButtonBounds(3); // Button after 3rd box
                    if (DrawButton(actionButton, "Submit Rating"))
                    {
                        strcpy(message, ""); // Clear messages
                        strcpy(resultText, "");
                        GuiRateService(); // Call logic
                    }
                    
                    DrawBackButton(GetScreenHeight() - 70, USER_CARE_MENU); // Back button
                    // V5 FIX: Message is now drawn globally at the bottom
                } break;
                
                // --- Support Sub-screens ---
                case USER_FAQS: // FAQs screen
                {
                    DrawScreenTitle("Frequently Asked Questions"); // Title
                    float textStartX = 150;
                    float textY = startY - 100;
                    float lineSpacing = 25;
                    float qSpacing = 40;

                    // Draw hardcoded FAQ text
                    DrawText("Q1: How do I reset my PIN?", textStartX, textY, 20, (Color){ 60, 90, 150, 255 });
                    DrawText("A: Go to User Menu > Change PIN", textStartX + 20, textY + lineSpacing, 20, DARKGRAY);
                    
                    textY += qSpacing + lineSpacing;
                    DrawText("Q2: What if my card is blocked?", textStartX, textY, 20, (Color){ 60, 90, 150, 255 });
                    DrawText("A: Use the 'Manage Card' option. If locked, contact staff.", textStartX + 20, textY + lineSpacing, 20, DARKGRAY);

                    textY += qSpacing + lineSpacing;
                    DrawText("Q3: How long does a transfer take?", textStartX, textY, 20, (Color){ 60, 90, 150, 255 });
                    DrawText("A: Transfers are instant within our bank.", textStartX + 20, textY + lineSpacing, 20, DARKGRAY);
                    
                    textY += qSpacing + lineSpacing;
                    DrawText("Q4: How do I check my transaction history?", textStartX, textY, 20, (Color){ 60, 90, 150, 255 });
                    DrawText("A: Go to User Menu > View Statement", textStartX + 20, textY + lineSpacing, 20, DARKGRAY);

                    textY += qSpacing + lineSpacing;
                    DrawText("Q5: What are the interest rates?", textStartX, textY, 20, (Color){ 60, 90, 150, 255 });
                    DrawText("A: Check Loan Menu > EMI Calculator for rates.", textStartX + 20, textY + lineSpacing, 20, DARKGRAY);
                    
                    DrawBackButton(GetScreenHeight() - 70, USER_SUPPORT_MENU); // Back button
                } break;

                case USER_SUPPORT: // Contact Support screen
                {
                    DrawScreenTitle("Help & Support"); // Title
                    float textStartX = 150;
                    float textY = startY - 100;
                    float lineSpacing = 40;

                    // Draw hardcoded contact info
                    DrawText("Email: daiict_bank@dau.ac.in", textStartX, textY, 30, DARKGRAY);
                    DrawText("Phone: +91 97140 58768", textStartX, textY + lineSpacing, 30, DARKGRAY);
                    DrawText("Hours: Mon-Fri, 9 AM - 6 PM", textStartX, textY + lineSpacing*2, 30, DARKGRAY);
                    DrawText("Branch: Visit nearest DAIICT Swiss Bank branch", textStartX, textY + lineSpacing*3, 30, DARKGRAY);

                    DrawBackButton(GetScreenHeight() - 70, USER_SUPPORT_MENU); // Back button
                } break;
            }

        EndDrawing(); // End drawing phase
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow(); // Close window and free resources
    //--------------------------------------------------------------------------------------

    return 0; // Exit program
}

//----------------------------------------------------------------------------------
// Module Functions Definition (GUI Helpers)
//----------------------------------------------------------------------------------

// Initializes/resets a text box struct
void InitTextBox(TextBox *box, Rectangle bounds, bool isPassword, int max)
{
    box->bounds = bounds;                         // Set position and size
    memset(box->text, 0, MAX_INPUT_LARGE + 1);    // Clear the text buffer
    box->charCount = 0;                           // Reset character count
    box->active = false;                          // Set to inactive
    box->isPassword = isPassword;                 // Set password flag
    box->maxChars = max;                          // Set max character limit
}

// Draws a standard button and returns true if clicked
bool DrawButton(Rectangle bounds, const char *text)
{
    bool clicked = false;
    Color bgColor = (Color){ 80, 120, 200, 255 }; // Normal blue
    Color fgColor = WHITE;                        // Text color

    if (CheckCollisionPointRec(mousePos, bounds)) // If mouse is hovering
    {
        bgColor = (Color){ 100, 150, 230, 255 }; // Lighter blue
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) // If clicked
        {
            clicked = true;
            bgColor = (Color){ 60, 90, 150, 255 }; // Darker blue
        }
    }

    DrawRectangleRounded(bounds, 0.2f, 4, bgColor); // Draw button rectangle
    int textWidth = MeasureText(text, 20); // Measure text
    DrawText(text, bounds.x + (bounds.width - textWidth) / 2, bounds.y + (bounds.height - 20) / 2, 20, fgColor); // Draw centered text

    return clicked; // Return click state
}

// ADDED: Special button for loan/complaint type selection
bool DrawSelectButton(Rectangle bounds, const char *text, bool selected)
{
    bool clicked = false;
    // Use blue if selected, gray if not
    Color bgColor = selected ? (Color){ 60, 90, 150, 255 } : (Color){ 200, 200, 200, 255 };
    Color fgColor = selected ? WHITE : (Color){ 80, 80, 80, 255 };

    if (CheckCollisionPointRec(mousePos, bounds)) // If mouse is hovering
    {
        bgColor = selected ? (Color){ 80, 120, 200, 255 } : (Color){ 220, 220, 220, 255 }; // Lighter shade
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) // If clicked
        {
            clicked = true;
            bgColor = (Color){ 60, 90, 150, 255 }; // Set to selected color
        }
    }

    DrawRectangleRounded(bounds, 0.2f, 4, bgColor); // Draw button
    int textWidth = MeasureText(text, 20);
    DrawText(text, bounds.x + (bounds.width - textWidth) / 2, bounds.y + (bounds.height - 20) / 2, 20, fgColor); // Draw text

    return clicked; // Return click state
}


// Draws the text box on screen
void DrawTextBox(TextBox *box, const char *placeholder)
{
    // Make textbox responsive (adjust width based on screen size)
    box->bounds.width = 500;
    if (GetScreenWidth() < 700) box->bounds.width = GetScreenWidth() - 100;
    box->bounds.x = (GetScreenWidth() - box->bounds.width) / 2; // Re-center

    DrawRectangleRec(box->bounds, WHITE); // Draw white background
    if (box->active) DrawRectangleLinesEx(box->bounds, 2, (Color){ 80, 120, 200, 255 }); // Thick blue border if active
    else DrawRectangleLinesEx(box->bounds, 1, GRAY); // Thin gray border if inactive

    if (box->charCount > 0) // If there is text
    {
        if (box->isPassword) // If it's a password
        {
            char passwordStars[MAX_INPUT_LARGE + 1] = { 0 }; // Buffer for stars
            for(int i = 0; i < box->charCount; i++) passwordStars[i] = '*'; // Fill with stars
            DrawText(passwordStars, box->bounds.x + 10, box->bounds.y + (box->bounds.height - 20) / 2, 20, BLACK); // Draw stars
        }
        else // If not password
        {
            // For large text boxes, draw text from top-left
            DrawText(box->text, box->bounds.x + 10, box->bounds.y + 15, 20, BLACK);
        }
    }
    else // If empty
    {
        DrawText(placeholder, box->bounds.x + 10, box->bounds.y + (box->bounds.height - 20) / 2, 20, LIGHTGRAY); // Draw placeholder
    }

    // --- Draw Blinking Cursor ---
    if (box->active && ((int)(GetTime() * 2.0f) % 2 == 0)) // If active and on blink cycle
    {
        float textWidth;
        if (box->isPassword) // If password
        {
            char passwordStars[MAX_INPUT_LARGE + 1] = { 0 };
            for(int i = 0; i < box->charCount; i++) passwordStars[i] = '*';
            textWidth = MeasureText(passwordStars, 20); // Measure width of stars
        }
        else
        {
            textWidth = MeasureText(box->text, 20); // Measure width of text
        }
        DrawRectangle(box->bounds.x + 10 + textWidth, box->bounds.y + 10, 2, box->bounds.height - 20, (Color){ 50, 50, 50, 255 }); // Draw cursor
    }
}

// Handles keyboard input for an active text box
void UpdateTextBox(TextBox *box)
{
    if (box->active) // Only if active
    {
        int key = GetCharPressed(); // Get key
        while (key > 0) // Process all keys
        {
            if ((key >= 32) && (box->charCount < box->maxChars)) // If printable and not full
            {
                box->text[box->charCount] = (char)key; // Add char
                box->text[box->charCount + 1] = '\0';  // Add null terminator
                box->charCount++;                      // Increment count
            }
            key = GetCharPressed(); // Get next key
        }
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) // If backspace
        {
            if (box->charCount > 0)
            {
                box->charCount--;                     // Decrement count
                box->text[box->charCount] = '\0';     // Move null terminator
            }
        }
    }
}

// Resets all text boxes to their initial empty state
void ClearAllTextBoxes(void)
{
    // Re-init all text boxes
    // InitTextBox(&pinVerifyBox, pinVerifyBox.bounds, pinVerifyBox.isPassword, pinVerifyBox.maxChars); // (REMOVED)
    InitTextBox(&transferUserBox, transferUserBox.bounds, transferUserBox.isPassword, transferUserBox.maxChars);
    InitTextBox(&transferNameBox, transferNameBox.bounds, transferNameBox.isPassword, transferNameBox.maxChars); // ADDED
    InitTextBox(&transferAmountBox, transferAmountBox.bounds, transferAmountBox.isPassword, transferAmountBox.maxChars);
    InitTextBox(&oldPinBox, oldPinBox.bounds, oldPinBox.isPassword, oldPinBox.maxChars); // ADDED
    InitTextBox(&pinBox, pinBox.bounds, pinBox.isPassword, pinBox.maxChars);
    InitTextBox(&confirmPinBox, confirmPinBox.bounds, confirmPinBox.isPassword, confirmPinBox.maxChars);
    // InitTextBox(&loanIdBox, loanIdBox.bounds, loanIdBox.isPassword, loanIdBox.maxChars); // REMOVED
    // InitTextBox(&loanTypeBox, loanTypeBox.bounds, loanTypeBox.isPassword, loanTypeBox.maxChars); // REMOVED
    InitTextBox(&loanAmountBox, loanAmountBox.bounds, loanAmountBox.isPassword, loanAmountBox.maxChars);
    InitTextBox(&loanTenureBox, loanTenureBox.bounds, loanTenureBox.isPassword, loanTenureBox.maxChars);
    InitTextBox(&emiAmountBox, emiAmountBox.bounds, emiAmountBox.isPassword, emiAmountBox.maxChars);
    InitTextBox(&emiRateBox, emiRateBox.bounds, emiRateBox.isPassword, emiRateBox.maxChars);
    InitTextBox(&emiTenureBox, emiTenureBox.bounds, emiTenureBox.isPassword, emiTenureBox.maxChars);
    InitTextBox(&fdAmountBox, fdAmountBox.bounds, fdAmountBox.isPassword, fdAmountBox.maxChars);
    InitTextBox(&fdTenureBox, fdTenureBox.bounds, fdTenureBox.isPassword, fdTenureBox.maxChars);
    InitTextBox(&fdCompoundBox, fdCompoundBox.bounds, fdCompoundBox.isPassword, fdCompoundBox.maxChars);
    // InitTextBox(&complaintCategoryBox, complaintCategoryBox.bounds, complaintCategoryBox.isPassword, complaintCategoryBox.maxChars); // REMOVED (V7)
    InitTextBox(&complaintDescBox, complaintDescBox.bounds, complaintDescBox.isPassword, complaintDescBox.maxChars);
    InitTextBox(&complaintIdBox, complaintIdBox.bounds, complaintIdBox.isPassword, complaintIdBox.maxChars);
    InitTextBox(&complaintNewDescBox, complaintNewDescBox.bounds, complaintNewDescBox.isPassword, complaintNewDescBox.maxChars);
    InitTextBox(&ratingBox, ratingBox.bounds, ratingBox.isPassword, ratingBox.maxChars);
    InitTextBox(&ratingFeedbackBox, ratingFeedbackBox.bounds, ratingFeedbackBox.isPassword, ratingFeedbackBox.maxChars);
}

// Draws a standard "Back" button
void DrawBackButton(float y, AppScreen returnsTo)
{
    Rectangle backButton = { 30, GetScreenHeight() - 70, 150, 40 }; // Default position
    if (y > 0) backButton.y = y; // Allow Y override
    
    if (DrawButton(backButton, "Back"))
    {
        currentScreen = returnsTo; // Go to specified screen
        strcpy(message, ""); // Clear messages
        strcpy(resultText, "");
        ClearAllTextBoxes(); // Reset forms
        showTransferConfirm = false; // V8 ADDED: Reset transfer state
    }
}

// Draws a standard centered screen title
void DrawScreenTitle(const char *title)
{
    int textWidth = MeasureText(title, 40);
    DrawText(title, (GetScreenWidth() - textWidth) / 2, 130, 40, (Color){ 50, 50, 50, 255 });
}

// Gets a standard button position below the lowest text box on the current screen
Rectangle GetStdButtonBounds(int position)
{
    float boxWidth = 500;
    float boxHeight = 50;
    float spacing = 20;
    float startX = (GetScreenWidth() - boxWidth) / 2; // Centered X
    float startY = 300; // Base start Y
    
    // Find highest Y of textboxes on the current screen
    switch(currentScreen)
    {
        case USER_TRANSFER: 
            if (!showTransferConfirm) startY = transferAmountBox.bounds.y; // V8 MODIFIED
            break;
        case USER_CHANGE_PIN: startY = confirmPinBox.bounds.y; break;
        case USER_LOAN_APPLY: startY = loanTenureBox.bounds.y; break;
        case USER_LOAN_EMI: startY = emiTenureBox.bounds.y; break;
        case USER_LOAN_FD: startY = fdCompoundBox.bounds.y; break;
        case USER_CARE_REGISTER: startY = complaintDescBox.bounds.y + complaintDescBox.bounds.height - boxHeight; break;
        case USER_CARE_UPDATE: startY = complaintNewDescBox.bounds.y + complaintNewDescBox.bounds.height - boxHeight; break;
        case USER_CARE_RATE: startY = ratingFeedbackBox.bounds.y + ratingFeedbackBox.bounds.height - boxHeight; break;
        // case USER_VERIFY_PIN: startY = pinVerifyBox.bounds.y; break; (REMOVED)
        default: break;
    }
    
    // Return the calculated rectangle
    return (Rectangle){ startX, startY + (boxHeight + spacing) * position, boxWidth, boxHeight };
}

//----------------------------------------------------------------------------------
// Module Functions Definition (File I/O - users.txt)
//----------------------------------------------------------------------------------

// Loads account from users.txt by username (account number) into the global `currentAccount`
bool LoadAccountByUsername(char* username)
{
    FILE *file = fopen(USER_FILE, "r"); // Open users.txt
    if (file == NULL) return false;

    bool found = false;
    Account acc;
    int isBlocked;

    // Read 4-column format
    while (fscanf(file, "%s %s %lf %d", acc.username, acc.password, &acc.balance, &isBlocked) != EOF)
    {
        if (strcmp(username, acc.username) == 0) // If this is our user
        {
            acc.isCardBlocked = (isBlocked == 1); // Convert int to bool
            currentAccount = acc; // Copy data to global struct
            found = true;
            break;
        }
    }

    fclose(file);
    return found; // Return true if found
}

// Checks if a user (account number) exists in users.txt
bool CheckUserExists(char* username)
{
    FILE *file = fopen(USER_FILE, "r");
    if (file == NULL) return false;
    bool found = false;
    Account acc;
    int isBlocked;
    while (fscanf(file, "%s %s %lf %d", acc.username, acc.password, &acc.balance, &isBlocked) != EOF)
    {
        if (strcmp(username, acc.username) == 0) // If found
        {
            found = true;
            break;
        }
    }
    fclose(file);
    return found;
}

// Rewrites users.txt with updated data for one account
bool UpdateAccountInFile(Account updatedAccount)
{
    Account accounts[MAX_ACCOUNTS]; // In-memory array
    int count = 0;
    FILE *file = fopen(USER_FILE, "r"); // Open for reading
    if (file == NULL) return false;

    int isBlocked;
    // Read all accounts from users.txt into memory
    while (fscanf(file, "%s %s %lf %d", accounts[count].username, accounts[count].password, &accounts[count].balance, &isBlocked) != EOF)
    {
        accounts[count].isCardBlocked = (isBlocked == 1);
        if (strcmp(accounts[count].username, updatedAccount.username) == 0) // If this is the account to update
        {
            accounts[count] = updatedAccount; // Replace it in the array
        }
        count++;
        if (count >= MAX_ACCOUNTS) break; // Stop if array full
    }
    fclose(file);

    file = fopen(USER_FILE, "w"); // Open for writing (truncates)
    if (file == NULL) return false;
    // Write all accounts from memory back to the file
    for (int i = 0; i < count; i++)
    {
        fprintf(file, "%s %s %.2f %d\n",
                accounts[i].username,
                accounts[i].password,
                accounts[i].balance,
                accounts[i].isCardBlocked ? 1 : 0); // Convert bool to int
    }
    fclose(file);
    return true; // Success
}

// Appends a transaction to the user's specific transaction file (e.g., "1001_transactions.txt")
bool AddTransactionToFile(const char* username, const char* type, double amount, const char* description)
{
    char filename[100];
    sprintf(filename, "%s_transactions.txt", username); // Create filename
    FILE *file = fopen(filename, "a"); // Open for appending
    if (file == NULL) return false;

    char desc_safe[100]; // Copy description
    strcpy(desc_safe, description);
    for(int i = 0; desc_safe[i]; i++) { // Sanitize: replace spaces with underscores for easy parsing
        if(desc_safe[i] == ' ') desc_safe[i] = '_';
    }
    fprintf(file, "%s %.2f %s\n", type, amount, desc_safe); // Write to file
    fclose(file);
    return true;
}

// Loads and reverses transactions from user's file into the global `statement` array
int LoadTransactions(const char* username)
{
    char filename[100];
    sprintf(filename, "%s_transactions.txt", username); // Get filename
    FILE *file = fopen(filename, "r"); // Open for reading
    if (file == NULL) return 0; // No file, 0 transactions

    int count = 0;
    memset(statement, 0, sizeof(statement)); // Clear array
    // Read 3-column format
    while (fscanf(file, "%s %lf %s", statement[count].type, &statement[count].amount, statement[count].description) != EOF)
    {
        for(int i = 0; statement[count].description[i]; i++) { // Un-sanitize: replace underscores with spaces
            if(statement[count].description[i] == '_') statement[count].description[i] = ' ';
        }
        count++;
        if (count >= MAX_TRANSACTIONS) break; // Stop if array full
    }
    fclose(file);

    // Reverse array so newest transactions are first
    for (int i = 0; i < count / 2; i++) {
        Transaction temp = statement[i];
        statement[i] = statement[count - 1 - i];
        statement[count - 1 - i] = temp;
    }
    return count; // Return number of transactions loaded
}


//----------------------------------------------------------------------------------
// Module Functions Definition (File I/O - accounts.csv)
//----------------------------------------------------------------------------------

// Retrieves a single account's details from accounts.csv
static StaffAccount getStaffAccount(int accNo) {
    FILE *fp = fopen(FILE_NAME, "r"); // Open for reading
    StaffAccount acc;
    memset(&acc, 0, sizeof(acc)); // Zero out struct
    if (!fp) return acc; // Return empty struct if file not found
    char line[1024];
    while (fgets(line, sizeof(line), fp)) { // Read line by line
        char copy[1024];
        strcpy(copy, line);
        char *t = strtok(copy, ","); // Get first token (ID)
        if (!t) continue;
        int id = atoi(t);
        if (id == accNo) { // If this is the account
            // Parse CSV tokens
            char *p = strtok(NULL, ","); if (p) { strncpy(acc.name, p, sizeof(acc.name)-1); acc.name[sizeof(acc.name)-1]='\0'; }
            p = strtok(NULL, ","); if (p) { strncpy(acc.pin, p, sizeof(acc.pin)-1); acc.pin[sizeof(acc.pin)-1]='\0'; }
            p = strtok(NULL, ","); if (p) acc.balance = atof(p); else acc.balance = 0.0;
            p = strtok(NULL, ","); if (p) acc.cardBlocked = atoi(p); else acc.cardBlocked = 0;
            p = strtok(NULL, ","); if (p) { strncpy(acc.cardNumber, p, sizeof(acc.cardNumber)-1); acc.cardNumber[sizeof(acc.cardNumber)-1]='\0'; }
            p = strtok(NULL, "\n"); if (p) { strncpy(acc.createdBy, p, sizeof(acc.createdBy)-1); acc.createdBy[sizeof(acc.createdBy)-1] = '\0'; } // Read to newline
            acc.accountNumber = id;
            break; // Stop searching
        }
    }
    fclose(fp);
    return acc; // Return populated (or empty) struct
}

// Rewrites the accounts.csv file with updated data for one account
static void updateStaffAccount(StaffAccount acc) {
    FILE *fp = fopen(FILE_NAME, "r"); // Open original for reading
    if (!fp) return;
    FILE *temp = fopen("temp.csv", "w"); // Open temp for writing
    if (!temp) { fclose(fp); return; }

    char line[1024];
    while (fgets(line, sizeof(line), fp)) { // Read line by line
        char copy[1024];
        strcpy(copy, line);
        char *tok = strtok(copy, ","); // Get first token (ID)
        if (!tok) continue;
        int id = atoi(tok);
        if (id == acc.accountNumber) { // If this is the account to update
            // Write updated data from struct
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


//----------------------------------------------------------------------------------
// Module Functions Definition (Main Transfer Logic)
//----------------------------------------------------------------------------------

// Performs the complete transfer, updating all relevant files
bool PerformTransfer(char* recipient, double amount)
{
    // 1. Load recipient from users.txt
    FILE *file = fopen(USER_FILE, "r");
    if (file == NULL) return false;
    Account recipientAccount;
    bool recipientFound = false;
    Account tempAcc;
    int isBlocked;
    while (fscanf(file, "%s %s %lf %d", tempAcc.username, tempAcc.password, &tempAcc.balance, &isBlocked) != EOF)
    {
        if (strcmp(recipient, tempAcc.username) == 0)
        {
            tempAcc.isCardBlocked = (isBlocked == 1);
            recipientAccount = tempAcc; // Store recipient's data
            recipientFound = true;
            break;
        }
    }
    fclose(file);
    if (!recipientFound) return false; // Recipient not in users.txt

    // 2. Update sender's balance in users.txt
    currentAccount.balance -= amount; // Subtract from global struct
    if (!UpdateAccountInFile(currentAccount)) return false; // Write change to users.txt

    // 3. Update recipient's balance in users.txt
    recipientAccount.balance += amount; // Add to recipient's struct
    if (!UpdateAccountInFile(recipientAccount)) // Write change to users.txt
    {
        printf("CRITICAL ERROR: Sender charged, but recipient update failed in users.txt.\n");
        // Try to roll back sender's balance
        currentAccount.balance += amount;
        UpdateAccountInFile(currentAccount);
        return false;
    }

    // 4. --- SYNC: Sync sender's balance to accounts.csv ---
    int senderAccNo = atoi(currentAccount.username);
    StaffAccount senderStaffAcc = getStaffAccount(senderAccNo); // Get sender's full data
    if (senderStaffAcc.accountNumber != 0) {
        senderStaffAcc.balance = currentAccount.balance; // Sync balance
        updateStaffAccount(senderStaffAcc); // Write to accounts.csv
    }

    // 5. --- SYNC: Sync recipient's balance to accounts.csv ---
    int recipAccNo = atoi(recipientAccount.username);
    StaffAccount recipStaffAcc = getStaffAccount(recipAccNo); // Get recipient's full data
    if (recipStaffAcc.accountNumber != 0) {
        recipStaffAcc.balance = recipientAccount.balance; // Sync balance
        updateStaffAccount(recipStaffAcc); // Write to accounts.csv
    }

    // 6. --- SYNC: Log to main transactions.csv ---
    FILE *tfp = fopen(TRANSACTION_FILE, "a"); // Open main log
    if (tfp) {
        char timeStr[64];
        getCurrentTimestamp(timeStr, sizeof(timeStr)); // Get timestamp
        // Log sender's transaction
        fprintf(tfp, "%d,Transfer Out,%.2f,%.2f,%s\n", senderAccNo, amount, senderStaffAcc.balance, timeStr);
        // Log recipient's transaction
        fprintf(tfp, "%d,Transfer In,%.2f,%.2f,%s\n", recipAccNo, amount, recipStaffAcc.balance, timeStr);
        fclose(tfp);
    }

    // 7. Log to individual user transaction files ("XXXX_transactions.txt")
    char descSender[100];
    sprintf(descSender, "Transfer to %s", recipient);
    AddTransactionToFile(currentAccount.username, "TRANSFER_OUT", amount, descSender);

    char descRecipient[100];
    sprintf(descRecipient, "Transfer from %s", currentAccount.username);
    AddTransactionToFile(recipient, "TRANSFER_IN", amount, descRecipient);

    return true; // Success
}

//----------------------------------------------------------------------------------
// Module Functions Definition (Ported from bank_system_with_care.c)
//----------------------------------------------------------------------------------

// --- Ported Helpers ---
// Gets the current date/time as a formatted string
static void getCurrentTimestamp(char *buffer, size_t size)
{
    time_t now = time(NULL); // Get current time
    struct tm *t = localtime(&now); // Convert to local time struct
    // Format the time into the buffer (e.g., "2023-10-27 14:30:00")
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

// Finds the next available complaint ID (max + 1)
static int getNextComplaintId(void) {
    FILE *fp = fopen(COMPLAINT_FILE, "r"); // Open complaints.csv
    if (!fp) return 1; // Start at 1 if file doesn't exist

    int maxId = 0;
    char line[2048];
    while (fgets(line, sizeof(line), fp)) { // Read line by line
        int id;
        if (sscanf(line, "%d,", &id) == 1 && id > maxId) // Parse just ID
            maxId = id; // Track max
    }
    fclose(fp);
    return maxId + 1; // Return next ID
}

// Finds the next available loan ID (max + 1, starting at 10001)
static int getNextLoanId(void) {
    FILE *fp = fopen(LOAN_FILE, "r"); // Open temp_loan.txt
    int maxId = 10000; // Start from 10001
    if (!fp) return maxId + 1; // Return 10001 if file doesn't exist

    char line[256];
    while (fgets(line, sizeof(line), fp)) { // Read line by line
        int id;
        if (sscanf(line, "%d", &id) == 1 && id > maxId) { // Parse just ID
            maxId = id; // Track max
        }
    }
    fclose(fp);
    return maxId + 1; // Return next ID
}

// Checks if an account exists in accounts.csv
static int accountExists(int accNo) {
    FILE *fp = fopen(FILE_NAME, "r"); // Open accounts.csv
    if (!fp) return 0; // File not found
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char copy[1024];
        strcpy(copy, line);
        char *t = strtok(copy, ","); // Get first token (ID)
        if (!t) continue;
        if (atoi(t) == accNo) { fclose(fp); return 1; } // Found
    }
    fclose(fp);
    return 0; // Not found
}
// --- End Ported Helpers ---


// --- GUI Logic Functions ---

// Logic for "Change PIN" button
static void GuiChangePin(void)
{
    strcpy(message, ""); // Clear message

    // --- V3 ADDED: Check old PIN ---
    if (strcmp(oldPinBox.text, currentAccount.password) != 0)
    {
        strcpy(message, "Error: Incorrect OLD PIN!");
        return;
    }
    // --- END ADDED ---

    // Validate new PIN
    if (strlen(pinBox.text) != 4 || strspn(pinBox.text, "0123456789") != 4)
    {
        strcpy(message, "Error: New PIN must be 4 digits.");
        return;
    }
    if (strcmp(pinBox.text, confirmPinBox.text) != 0)
    {
        strcpy(message, "Error: PINs do not match.");
        return;
    }

    // 1. Update users.txt
    strcpy(currentAccount.password, pinBox.text); // Update global struct
    if (!UpdateAccountInFile(currentAccount)) // Write change to users.txt
    {
        strcpy(message, "Error: Failed to update login file.");
        return;
    }

    // 2. Update accounts.csv (sync)
    StaffAccount staffAcc = getStaffAccount(atoi(currentAccount.username)); // Get full data
    if (staffAcc.accountNumber != 0)
    {
        strcpy(staffAcc.pin, pinBox.text); // Sync PIN
        updateStaffAccount(staffAcc); // Write change to accounts.csv
    }

    strcpy(message, "PIN Updated Successfully!"); // Set success message
    currentScreen = USER_DASHBOARD; // Go to dashboard
    ClearAllTextBoxes(); // Reset form
}

// Logic for "Apply Loan" button
static void GuiApplyLoan(void)
{
    strcpy(message, "");
    strcpy(resultText, "");

    // --- V8 USER REQUEST: Check active loan count ---
    GuiLoanDetails(); // Load `myLoans` array with current user's loans
    int activeLoanCount = 0;
    for (int i = 0; i < myLoansCount; i++) {
        // Count loans that are "Pending" or "Approved"
        if (strcmp(myLoans[i].status, "Pending") == 0 || strcmp(myLoans[i].status, "Approved") == 0) {
            activeLoanCount++;
        }
    }

    if (activeLoanCount >= 2) { // If 2 or more
        strcpy(message, "Error: You already have 2 active loans. Cannot apply for a third.");
        return;
    }
    // --- END V8 USER REQUEST ---

    struct Loan l; // New loan struct
    l.acc_no = atoi(currentAccount.username); // User's account number
    l.loan_id = getNextLoanId(); // Auto-generated ID (e.g., 10001)
    l.type = selectedLoanType; // From button (1-6)
    l.amount = atof(loanAmountBox.text); // Get amount
    l.tenure = atoi(loanTenureBox.text); // Get tenure
    
    if (l.type == 0) // Check if type was selected
    {
        strcpy(message, "Error: Please select a loan type.");
        return;
    }
    if (l.amount <= 0 || l.tenure <= 0) // Check for valid numbers
    {
        strcpy(message, "Error: Amount and Tenure must be valid positive numbers.");
        return;
    }

    // Set interest rate based on type
    switch (l.type) {
        case 1: l.rate = 8.0;  break; // Home
        case 2: l.rate = 10.0; break; // Car
        case 3: l.rate = 15.0; break; // Gold
        case 4: l.rate = 17.0; break; // Personal
        case 5: l.rate = 13.0; break; // Business
        case 6: l.rate = 11.0; break; // Education
        default: // Should not happen
            strcpy(message, "Error: Invalid loan type selected.");
            return;
    }
    
    // Calculate EMI
    float R = l.rate / (12 * 100); // Monthly rate
    l.emi = (l.amount * R * pow(1 + R, l.tenure)) / (pow(1 + R, l.tenure) - 1);
    strcpy(l.status, "Pending"); // V4 MODIFIED: Set to "Pending" for admin approval

    FILE *fp = fopen(LOAN_FILE, "a"); // Open temp_loan.txt for appending
    if (fp == NULL) {
        strcpy(message, "Error: Could not open loan file.");
        return;
    }
    // Write new loan in 8-column format
    fprintf(fp, "%d %d %d %.2f %.2f %d %.2f %s\n",
            l.loan_id, l.acc_no, l.type, l.amount,
            l.rate, l.tenure, l.emi, l.status);
    fclose(fp);
    
    // Set success message
    sprintf(resultText, "Loan Submitted! Your application (ID: %d) is pending approval.", l.loan_id);
    strcpy(message, "");
    ClearAllTextBoxes();
    selectedLoanType = 0; // Reset button selection
}

// Loads user's loans from temp_loan.txt into the global `myLoans` array
static void GuiLoanDetails(void)
{
    myLoansCount = 0; // Reset count
    FILE *fp = fopen(LOAN_FILE, "r"); // Open temp_loan.txt
    if (fp == NULL) return; // File not found
    
    int myAccNo = atoi(currentAccount.username); // Get this user's ID
    struct Loan l;
    char line[256];
    
    while (fgets(line, sizeof(line), fp)) // Read line by line
    {
        // Parse 8-column format
        if (sscanf(line, "%d %d %d %f %f %d %f %19s",
                   &l.loan_id, &l.acc_no, &l.type, &l.amount,
                   &l.rate, &l.tenure, &l.emi, l.status) == 8)
        {
            if (l.acc_no == myAccNo) // If this loan belongs to this user
            {
                myLoans[myLoansCount] = l; // Add to array
                myLoansCount++; // Increment count
                if (myLoansCount >= MAX_LOANS) break; // Stop if array full
            }
        }
    }
    fclose(fp);
}

// Logic for "EMI Calc" button
static void GuiEmiCalc(void)
{
    strcpy(message, "");
    strcpy(resultText, "");
    float Loan = atof(emiAmountBox.text);
    float annualRate = atof(emiRateBox.text);
    int N = atoi(emiTenureBox.text); // Tenure in months
    
    if (Loan <= 0 || annualRate <= 0 || N <= 0) // Validate
    {
        strcpy(message, "Error: All fields must be valid positive numbers.");
        return;
    }

    float R = annualRate / (12 * 100); // Monthly rate
    float EMI = (Loan * R * pow(1 + R, N)) / (pow(1 + R, N) - 1); // EMI formula
    float Total_interest = EMI * N - Loan; // Total interest paid
    
    sprintf(resultText, "Monthly EMI: Rs %.2f | Total Interest: Rs %.2f", EMI, Total_interest); // Set success
    strcpy(message, "");
    ClearAllTextBoxes(); // Reset form
}

// Logic for "FD Calc" button
static void GuiFixDeposit(void)
{
    strcpy(message, "");
    strcpy(resultText, "");
    struct fd a; // FD struct
    a.Principal = atof(fdAmountBox.text);
    a.years = atoi(fdTenureBox.text);
    a.n = atoi(fdCompoundBox.text); // Compounding periods (1, 4, or 12)
    a.Rate = 7.0; // Fixed rate

    // Validate
    if (a.Principal <= 0 || a.years <= 0 || (a.n != 1 && a.n != 4 && a.n != 12))
    {
        strcpy(message, "Error: Invalid inputs. Compounding must be 1, 4, or 12.");
        return;
    }
    
    // FD formula: A = P * (1 + R/(n*100))^(n*t)
    a.A = a.Principal * pow((1 + a.Rate / (a.n * 100)), a.n * a.years);

    FILE *fd_file = fopen(FD_FILE, "a"); // Open fd.txt for appending
    if (fd_file)
    {
        // Log the FD record
        fprintf(fd_file," %d %.2f %.2f %d %d %.2f %.2f\n",
                atoi(currentAccount.username), // Added acc no
                a.Principal, a.Rate, a.years, a.n, a.A, a.A - a.Principal);
        fclose(fd_file);
    }
    
    sprintf(resultText, "Maturity Amount: Rs %.2f | Total Interest: Rs %.2f", a.A, a.A - a.Principal); // Set success
    strcpy(message, "");
    ClearAllTextBoxes(); // Reset form
}

// Logic for "Register Complaint" button
static void GuiRegisterComplaint(void)
{
    strcpy(message, "");
    FILE *fp = fopen(COMPLAINT_FILE, "a"); // Open complaints.csv for appending
    if (!fp) { strcpy(message, "Error: Cannot open complaints file."); return; }

    Complaint c; // New complaint struct
    c.complaintId = getNextComplaintId(); // Get next ID
    c.accountNumber = atoi(currentAccount.username); // This user's ID
    
    // --- V7 MODIFICATION ---
    int cat = selectedComplaintType; // Get from button (1-6)
    if (cat == 0) // If no button selected
    {
        strcpy(message, "Error: Please select a complaint type.");
        fclose(fp);
        return;
    }
    // --- END V7 MODIFICATION ---

    // Convert category (int) to string
    switch(cat) {
        case 1: strcpy(c.category, "Card Issues"); break;
        case 2: strcpy(c.category, "Transaction Problems"); break;
        case 3: strcpy(c.category, "Account Access"); break;
        case 4: strcpy(c.category, "Loan Issues"); break;
        case 5: strcpy(c.category, "Staff Behavior"); break;
        case 6: strcpy(c.category, "Other"); break;
        default:
            // Should not be reachable
            strcpy(message, "Error: Invalid category.");
            fclose(fp);
            return;
    }
    
    if (complaintDescBox.charCount == 0) // Check for empty description
    {
        strcpy(message, "Error: Description cannot be empty.");
        fclose(fp);
        return;
    }
    strncpy(c.description, complaintDescBox.text, sizeof(c.description)-1); // Copy description
    
    strcpy(c.status, "Pending"); // Set status
    strcpy(c.response, "N/A"); // Default response
    getCurrentTimestamp(c.timestamp, sizeof(c.timestamp)); // Get timestamp

    // Write new complaint as CSV line
    fprintf(fp, "%d,%d,%s,%s,%s,%s,%s\n", c.complaintId, c.accountNumber,
            c.category, c.description, c.status, c.response, c.timestamp);
    fclose(fp);
    
    sprintf(resultText, "Complaint registered! Your ID is: %d", c.complaintId); // Set success (V5 FIX)
    currentScreen = USER_CARE_MENU; // Go back to menu
    ClearAllTextBoxes(); // Reset form
    selectedComplaintType = 0; // V7 ADDED: Reset button selection
}

// Loads user's complaints from complaints.csv into the global `myComplaints` array
static void GuiViewMyComplaints(void)
{
    myComplaintsCount = 0; // Reset count
    FILE *fp = fopen(COMPLAINT_FILE, "r"); // Open complaints.csv
    if (!fp) return; // File not found
    
    int myAccNo = atoi(currentAccount.username); // This user's ID
    char line[2048]; // Line buffer
    
    while (fgets(line, sizeof(line), fp)) // Read line by line
    {
        Complaint c;
        // Parse CSV line
        if (sscanf(line, "%d,%d,%49[^,],%499[^,],%19[^,],%499[^,],%63[^\n]",
                   &c.complaintId, &c.accountNumber, c.category,
                   c.description, c.status, c.response, c.timestamp) >= 6)
        {
            if (c.accountNumber == myAccNo) // If this complaint belongs to this user
            {
                myComplaints[myComplaintsCount] = c; // Add to array
                myComplaintsCount++; // Increment count
                if (myComplaintsCount >= MAX_COMPLAINTS) break; // Stop if array full
            }
        }
    }
    fclose(fp);
}

// Logic for "Update Complaint" button
static void GuiUpdateComplaint(void)
{
    strcpy(message, "");
    int compId = atoi(complaintIdBox.text); // Get ID from text box
    if (compId <= 0)
    {
        strcpy(message, "Error: Invalid Complaint ID.");
        return;
    }
    if (complaintNewDescBox.charCount == 0) // Check for empty description
    {
        strcpy(message, "Error: New description cannot be empty.");
        return;
    }

    FILE *fp = fopen(COMPLAINT_FILE, "r"); // Open original for reading
    FILE *temp = fopen("temp_complaints.csv", "w"); // Open temp for writing
    if (!fp || !temp) {
        strcpy(message, "Error: File system error.");
        if (fp) fclose(fp);
        if (temp) fclose(temp);
        return;
    }

    char line[2048]; // Line buffer
    int found = 0; // Flag
    int myAccNo = atoi(currentAccount.username); // This user's ID

    while (fgets(line, sizeof(line), fp)) { // Read line by line
        Complaint c;
        if (sscanf(line, "%d,%d,%49[^,],%499[^,],%19[^,],%499[^,],%63[^\n]", // Parse
                   &c.complaintId, &c.accountNumber, c.category,
                   &c.description, &c.status, &c.response, &c.timestamp) >= 6) {
            
            // Check if ID matches, account matches, AND status is "Pending"
            if (c.complaintId == compId && c.accountNumber == myAccNo && strcmp(c.status, "Pending") == 0) {
                found = 1;
                strncpy(c.description, complaintNewDescBox.text, sizeof(c.description)-1); // Update description
                // Write updated line to temp file
                fprintf(temp, "%d,%d,%s,%s,%s,%s,%s\n", c.complaintId, c.accountNumber,
                        c.category, c.description, c.status, c.response, c.timestamp);
            } else {
                fputs(line, temp); // Not the target, write original line
            }
        } else {
            fputs(line, temp); // Keep lines that fail to parse
        }
    }

    fclose(fp); // Close original
    fclose(temp); // Close temp
    remove(COMPLAINT_FILE); // Delete original
    rename("temp_complaints.csv", COMPLAINT_FILE); // Rename temp to original
    
    if (found) {
        strcpy(resultText, "Complaint updated successfully!"); // Set success (V5 FIX)
        currentScreen = USER_CARE_MENU; // Go back to menu
        ClearAllTextBoxes(); // Reset form
    } else {
        strcpy(message, "Error: Complaint not found, not yours, or already processed."); // Set error
    }
}

// Logic for "Rate Service" button
static void GuiRateService(void)
{
    strcpy(message, "");
    FILE *fp = fopen(RATING_FILE, "a"); // Open ratings.csv for appending
    if (!fp) { strcpy(message, "Error: Cannot open ratings file."); return; }

    Rating r; // New rating struct
    r.accountNumber = atoi(currentAccount.username); // This user's ID
    r.rating = atoi(ratingBox.text); // Get rating
    
    if (r.rating < 1 || r.rating > 5) // Validate rating
    {
        strcpy(message, "Error: Rating must be between 1 and 5.");
        fclose(fp);
        return;
    }
    
    if (ratingFeedbackBox.charCount > 0) // If feedback provided
    {
        strncpy(r.feedback, ratingFeedbackBox.text, sizeof(r.feedback)-1); // Copy feedback
    } else {
        strcpy(r.feedback, "N/A"); // Default feedback
    }

    getCurrentTimestamp(r.timestamp, sizeof(r.timestamp)); // Get timestamp
    fprintf(fp, "%d,%d,%s,%s\n", r.accountNumber, r.rating, r.feedback, r.timestamp); // Write to file
    fclose(fp);
    
    strcpy(resultText, "Thank you for your feedback!"); // Set success (V5 FIX)
    currentScreen = USER_CARE_MENU; // Go back to menu
    ClearAllTextBoxes(); // Reset form
}