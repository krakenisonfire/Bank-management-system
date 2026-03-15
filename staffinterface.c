/*******************************************************************************************
*
* DAIICT Bank - Staff Menu (Raylib)
*
* This program provides the staff-facing GUI for the banking system.
* It is intended to be launched by 'login.exe' and will receive the
* staff's username as a command-line argument.
*
* It manages:
* - User account creation
* - Deposits and withdrawals
* - PIN resets
* - Complaint viewing and resolution
*
* All actions are logged to 'changelog.csv' under the logged-in staff's name.
*
* Compile example (Windows):
* gcc staff_menu.c -o staff_menu.exe -lraylib -lgdi32 -lwinmm -Wl,-subsystem,windows
*
* Data Files Used:
* - accounts.csv (Primary data)
* - users.txt (Login/balance data for user app)
* - transactions.csv
* - changelog.csv
* - complaints.csv
*
********************************************************************************************/

#include "raylib.h"     // Core Raylib library for GUI, drawing, and input
#include <stdio.h>      // Standard input/output for file operations (fopen, printf, etc.)
#include <string.h>     // String manipulation functions (strcpy, strcmp, etc.)
#include <stdlib.h>     // Standard library for system(), atoi(), atof(), rand()
#include <time.h>       // For generating timestamps and seeding rand()
#include <math.h>       // Math functions (not currently used, but included)

//----------------------------------------------------------------------------------
// Defines and Types
//----------------------------------------------------------------------------------
#define SCREEN_WIDTH 1680   // Define the width of the application window
#define SCREEN_HEIGHT 920   // Define the height of the application window

#define MAX_INPUT_CHARS 500 // Increased size for large text boxes (complaint descriptions)
#define MAX_COMPLAINTS 256  // Max complaints to load into memory for viewing
#define MAX_LOGIN_CHARS 50  // Max chars for login fields (compatibility with other files)
#define MAX_ACCOUNTS 100    // Max accounts for in-memory users.txt array

// File names for data
#define FILE_NAME           "accounts.csv"      // Main customer accounts database (created by staff)
#define USER_FILE           "users.txt"         // File for customer login credentials (synced with accounts.csv)
#define TRANSACTION_FILE    "transactions.csv"  // Log of all transactions
#define CHANGELOG_FILE      "changelog.csv"     // Log of staff actions (e.g., account creation)
#define COMPLAINT_FILE      "complaints.csv"    // Customer complaints log

// Enum to manage which screen is currently displayed
typedef enum {
    STAFF_MAIN,         // The main navigation menu
    STAFF_CREATE,       // Screen to create a new customer account
    STAFF_DEPOSIT,      // Screen to deposit funds
    STAFF_WITHDRAW,     // Screen to withdraw funds
    STAFF_FORGOT_PIN,   // Screen to reset a customer's PIN
    STAFF_VIEW_PENDING, // Screen to view pending complaints
    STAFF_VIEW_RESOLVED,// Screen to view resolved complaints
    STAFF_RESPOND       // Screen to respond to a complaint (and mark as resolved)
} AppScreen;

// Account struct for accounts.csv (main customer database)
typedef struct {
    int accountNumber;      // Unique account number
    char name[100];         // Customer's full name
    char pin[10];           // Customer's PIN
    double balance;         // Current balance
    int cardBlocked;        // Block status (1 for blocked, 0 for active)
    char cardNumber[24];    // Customer's card number
    char createdBy[50];     // Staff ID of the creator
} Account;

// --- ADDED: Struct to match users.txt format (customer login file) ---
typedef struct {
    char username[MAX_LOGIN_CHARS + 1]; // This will be account number as string
    char password[MAX_LOGIN_CHARS + 1]; // This will be PIN
    double balance;                     // Current balance
    int isCardBlocked;                  // Block status (1 or 0)
} UserFileAccount;

// Complaint struct for complaints.csv
typedef struct {
    int complaintId;        // Unique ID for the complaint
    int accountNumber;      // Account that filed the complaint
    char category[50];      // Category of complaint (e.g., "Billing", "Service")
    char description[500];  // The user's complaint text
    char status[20];        // "Pending", "In Progress", "Resolved"
    char response[500];     // The staff's response text
    char timestamp[64];     // Date and time the complaint was filed
} Complaint;

// GUI Text input box struct
typedef struct {
    Rectangle bounds;                   // Position and size of the text box
    char text[MAX_INPUT_CHARS + 1];     // The text content
    int charCount;                      // Current number of characters
    bool active;                        // Is the box currently selected?
    bool isPassword;                    // Should the text be masked (e.g., "********")?
} TextBox;

//----------------------------------------------------------------------------------
// Global Variables
//----------------------------------------------------------------------------------
static AppScreen currentScreen = STAFF_MAIN;    // Tracks the current active screen
static Vector2 mousePos = { 0.0f, 0.0f };       // Stores the mouse's current (x, y) position
static char message[200] = { 0 };               // Stores feedback messages (e.g., "Success: ...")
static char loggedInStaff[51] = "staff_unknown"; // Stores staff username (passed from login.exe)
static Vector2 scrollPending = { 0, 0 };      // Scroll Y-offset for pending complaints list
static Vector2 scrollResolved = { 0, 0 };     // Scroll Y-offset for resolved complaints list

// TextBoxes for all forms
static TextBox createNameBox;       // For "Create Account" -> Name
static TextBox createPinBox;        // For "Create Account" -> PIN
static TextBox createDepositBox;    // For "Create Account" -> Initial Deposit
static TextBox depositAccNoBox;     // For "Deposit" -> Account Number
static TextBox depositAmountBox;    // For "Deposit" -> Amount
static TextBox withdrawAccNoBox;    // For "Withdraw" -> Account Number
static TextBox withdrawAmountBox;   // For "Withdraw" -> Amount
static TextBox forgotPinAccNoBox;   // For "Reset PIN" -> Account Number
static TextBox forgotPinNewPinBox;  // For "Reset PIN" -> New PIN
static TextBox respondIdBox;        // For "Respond" -> Complaint ID
static TextBox respondResponseBox;  // For "Respond" -> Response Text (large box)

// Arrays to hold loaded complaints for viewing
static Complaint pendingComplaints[MAX_COMPLAINTS];  // In-memory array for pending complaints
static Complaint resolvedComplaints[MAX_COMPLAINTS]; // In-memory array for resolved complaints
static int pendingCount = 0;                         // Count of pending complaints in array
static int resolvedCount = 0;                        // Count of resolved complaints in array

//----------------------------------------------------------------------------------
// Module Functions Declaration (GUI Helpers from login.c)
//----------------------------------------------------------------------------------
static void InitTextBox(TextBox *box, Rectangle bounds, bool isPassword); // Initializes/resets a text box
static bool DrawButton(Rectangle bounds, const char *text);               // Draws a button and returns true if clicked
static void DrawTextBox(TextBox *box, const char *placeholder);           // Draws a text box (and placeholder)
static void UpdateTextBox(TextBox *box);                                  // Handles keyboard input for a text box
static void ClearAllTextBoxes(void);                                      // Resets all text boxes on the screen
static void ActivateClickedTextBox(void);                                 // Activates the text box that was clicked

//----------------------------------------------------------------------------------
// Module Functions Declaration (Ported Backend Logic)
//----------------------------------------------------------------------------------
static void getCurrentTimestamp(char *buffer, size_t size);              // Gets the current date/time as a string
static int nextAccountNumber(void);                                      // Finds the next available account number
static int accountExists(int accNo);                                     // Checks if an account exists in accounts.csv
static Account getAccount(int accNo);                                    // Reads a single account's details from accounts.csv
static void updateAccount(Account acc);                                  // Rewrites a single account's details in accounts.csv
static void logTransaction(int accNo, const char *type, double amount, double balance); // Appends a transaction to transactions.csv
static void logChange(const char *staff, const char *action, int accNo, double amount); // Appends a staff action to changelog.csv
static void generateCardNumber(char *cardNumber);                        // Generates a random 16-digit card number
static int getNextComplaintId(void);                                     // Finds the next available complaint ID

// --- ADDED: Functions to sync with users.txt ---
static UserFileAccount GetUserFileAccount(const char* username); // Reads a single user's details from users.txt
static bool UpdateUserFile(UserFileAccount updatedAccount);      // Rewrites a single user's details in users.txt
static int usernameExistsInUserFile(const char* username);       // Checks if a username (account number) exists in users.txt

//----------------------------------------------------------------------------------
// Module Functions Declaration (New GUI-driven Logic)
//----------------------------------------------------------------------------------
static void GuiCreateAccount(void);         // Logic handler for "Create Account" button
static void GuiDeposit(void);               // Logic handler for "Deposit" button
static void GuiWithdraw(void);              // Logic handler for "Withdraw" button
static void GuiForgotPin(void);             // Logic handler for "Reset PIN" button
static void GuiRespondToComplaint(void);    // Logic handler for "Respond" button
static void LoadComplaints(void);           // Reads complaints.csv into the in-memory arrays
static void DrawComplaintsScreen(bool drawPending); // Draws the scrolling list of complaints

//----------------------------------------------------------------------------------
// Main Entry Point
//----------------------------------------------------------------------------------
int main(int argc, char *argv[]) // argc: argument count, argv: argument vector
{
    // Initialization
    //--------------------------------------------------------------------------------------
    if (argc<3) return 0;
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "DAIICT Bank - Staff Menu"); // Create the window

    if (argc > 2) { // Check if a command-line argument was passed
        strncpy(loggedInStaff, argv[1], 50); // Copy the first argument (staff username)
        loggedInStaff[50] = '\0'; // Ensure null termination
    }

    srand((unsigned)time(NULL)); // Seed the random number generator (for card numbers)

    // --- Define layout for text boxes ---
    float boxWidth = 500;
    float boxHeight = 50;
    float spacing = 20;
    float startX = (SCREEN_WIDTH - boxWidth) / 2; // Centered X
    float startY = 200; // Starting Y for forms

    // --- Initialize all text boxes ---
    InitTextBox(&createNameBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false);
    InitTextBox(&createPinBox, (Rectangle){ startX, startY + boxHeight + spacing, boxWidth, boxHeight }, true); // Is a password
    InitTextBox(&createDepositBox, (Rectangle){ startX, startY + (boxHeight + spacing) * 2, boxWidth, boxHeight }, false);

    InitTextBox(&depositAccNoBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false);
    InitTextBox(&depositAmountBox, (Rectangle){ startX, startY + boxHeight + spacing, boxWidth, boxHeight }, false);

    InitTextBox(&withdrawAccNoBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false);
    InitTextBox(&withdrawAmountBox, (Rectangle){ startX, startY + boxHeight + spacing, boxWidth, boxHeight }, false);

    InitTextBox(&forgotPinAccNoBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false);
    InitTextBox(&forgotPinNewPinBox, (Rectangle){ startX, startY + boxHeight + spacing, boxWidth, boxHeight }, true); // Is a password

    InitTextBox(&respondIdBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false);
    InitTextBox(&respondResponseBox, (Rectangle){ startX, startY + boxHeight + spacing, boxWidth, 300 }, false); // Large text box


    SetTargetFPS(60); // Set target FPS
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) // Loop until window is closed
    {
        // Update
        //----------------------------------------------------------------------------------
        mousePos = GetMousePosition(); // Get mouse position every frame

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { // On click
            ActivateClickedTextBox(); // Check if a text box was clicked
        }

        // Update the active text box (only one can be active)
        if (createNameBox.active) UpdateTextBox(&createNameBox);
        else if (createPinBox.active) UpdateTextBox(&createPinBox);
        else if (createDepositBox.active) UpdateTextBox(&createDepositBox);
        else if (depositAccNoBox.active) UpdateTextBox(&depositAccNoBox);
        else if (depositAmountBox.active) UpdateTextBox(&depositAmountBox);
        else if (withdrawAccNoBox.active) UpdateTextBox(&withdrawAccNoBox);
        else if (withdrawAmountBox.active) UpdateTextBox(&withdrawAmountBox);
        else if (forgotPinAccNoBox.active) UpdateTextBox(&forgotPinAccNoBox);
        else if (forgotPinNewPinBox.active) UpdateTextBox(&forgotPinNewPinBox);
        else if (respondIdBox.active) UpdateTextBox(&respondIdBox);
        else if (respondResponseBox.active) UpdateTextBox(&respondResponseBox);

        // Handle mouse wheel scrolling for complaint lists
        if (currentScreen == STAFF_VIEW_PENDING) {
            scrollPending.y += GetMouseWheelMove() * 20; // Adjust scroll based on wheel
            if (scrollPending.y > 0) scrollPending.y = 0; // Don't scroll past the top
        } else if (currentScreen == STAFF_VIEW_RESOLVED) {
            scrollResolved.y += GetMouseWheelMove() * 20;
            if (scrollResolved.y > 0) scrollResolved.y = 0;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing(); // Start drawing phase

            ClearBackground((Color){ 245, 245, 245, 255 }); // Light gray background

            // --- Draw common header ---
            DrawRectangle(0, 0, SCREEN_WIDTH, 80, (Color){ 60, 90, 150, 255 }); // Dark blue bar
            DrawText("Staff Menu", 30, 20, 40, WHITE); // Title
            char welcomeText[100];
            sprintf(welcomeText, "Logged in as: %s", loggedInStaff); // Welcome message
            DrawText(welcomeText, SCREEN_WIDTH - MeasureText(welcomeText, 20) - 30, 30, 20, WHITE); // Draw top-right
            // --- End Header ---

            // --- Define button layout for main menu ---
            float menuButtonWidth = 300;
            float menuButtonHeight = 60;
            float menuCol1X = (SCREEN_WIDTH / 2) - menuButtonWidth - 50; // Column 1 X
            float menuCol2X = (SCREEN_WIDTH / 2) + 50;                   // Column 2 X
            float menuRow1Y = 200;
            float menuRow2Y = menuRow1Y + menuButtonHeight + spacing;
            float menuRow3Y = menuRow2Y + menuButtonHeight + spacing;
            float menuRow4Y = menuRow3Y + menuButtonHeight + spacing;

            Rectangle createButton = { menuCol1X, menuRow1Y, menuButtonWidth, menuButtonHeight };
            Rectangle depositButton = { menuCol1X, menuRow2Y, menuButtonWidth, menuButtonHeight };
            Rectangle withdrawButton = { menuCol1X, menuRow3Y, menuButtonWidth, menuButtonHeight };
            Rectangle forgotPinButton = { menuCol1X, menuRow4Y, menuButtonWidth, menuButtonHeight };

            Rectangle viewPendingButton = { menuCol2X, menuRow1Y, menuButtonWidth, menuButtonHeight };
            Rectangle viewResolvedButton = { menuCol2X, menuRow2Y, menuButtonWidth, menuButtonHeight };
            Rectangle respondButton = { menuCol2X, menuRow3Y, menuButtonWidth, menuButtonHeight };
            Rectangle logoutButton = { menuCol2X, menuRow4Y, menuButtonWidth, menuButtonHeight };
            // --- End menu button layout ---

            // --- Define button layout for forms ---
            Rectangle backButton = { startX, 650, (boxWidth / 2) - 10, 50 }; // Half-width Back
            Rectangle submitButton = { startX + (boxWidth / 2) + 10, 650, (boxWidth / 2) - 10, 50 }; // Half-width Submit
            Rectangle backButtonFull = { startX, 650, boxWidth, 50 }; // Full-width Back (for complaint view)
            
            // Buttons for the "Respond" screen (different layout due to large text box)
            Rectangle submitRespondButton = { startX + (boxWidth / 2) + 10, startY + 300 + boxHeight + spacing + 10, (boxWidth / 2) - 10, 50 };
            Rectangle backRespondButton = { startX, startY + 300 + boxHeight + spacing + 10, (boxWidth / 2) - 10, 50 };
            // --- End form button layout ---


            switch (currentScreen) // Draw content based on the current screen
            {
                case STAFF_MAIN: // Main menu screen
                {
                    DrawText("Staff Dashboard", (SCREEN_WIDTH - MeasureText("Staff Dashboard", 60)) / 2, 120, 60, (Color){ 50, 50, 50, 255 }); // Title

                    // Draw all menu buttons and handle clicks
                    if (DrawButton(createButton, "Create Account")) {
                        currentScreen = STAFF_CREATE; // Go to Create screen
                        ClearAllTextBoxes();          // Reset form
                    }
                    if (DrawButton(depositButton, "Deposit Money")) {
                        currentScreen = STAFF_DEPOSIT; // Go to Deposit screen
                        ClearAllTextBoxes();           // Reset form
                    }
                    if (DrawButton(withdrawButton, "Withdraw Money")) {
                        currentScreen = STAFF_WITHDRAW; // Go to Withdraw screen
                        ClearAllTextBoxes();            // Reset form
                    }
                    if (DrawButton(forgotPinButton, "Forgot PIN (Reset)")) {
                        currentScreen = STAFF_FORGOT_PIN; // Go to Reset PIN screen
                        ClearAllTextBoxes();              // Reset form
                    }
                    if (DrawButton(viewPendingButton, "View Pending Complaints")) {
                        LoadComplaints(); // Load data from file into array
                        scrollPending.y = 0; // Reset scroll
                        currentScreen = STAFF_VIEW_PENDING; // Go to Pending screen
                        ClearAllTextBoxes();
                    }
                    if (DrawButton(viewResolvedButton, "View Resolved Complaints")) {
                        LoadComplaints(); // Load data from file into array
                        scrollResolved.y = 0; // Reset scroll
                        currentScreen = STAFF_VIEW_RESOLVED; // Go to Resolved screen
                        ClearAllTextBoxes();
                    }
                    if (DrawButton(respondButton, "Respond to Complaint")) {
                        currentScreen = STAFF_RESPOND; // Go to Respond screen
                        ClearAllTextBoxes();           // Reset form
                    }
                    if (DrawButton(logoutButton, "Logout")) {
                        system("start login.exe"); // Relaunch the login program
                        CloseWindow(); // Close this (staff) program
                        return 0;      // Exit main loop
                    }
                } break;

                case STAFF_CREATE: // Create Account form
                {
                    DrawText("Create New User Account", startX, startY - 70, 40, (Color){ 50, 50, 50, 255 }); // Title
                    DrawTextBox(&createNameBox, "Full Name");
                    DrawTextBox(&createPinBox, "4-Digit PIN");
                    DrawTextBox(&createDepositBox, "Initial Deposit Amount");

                    if (DrawButton(backButton, "Back")) {
                        currentScreen = STAFF_MAIN; // Go to main menu
                        strcpy(message, "");        // Clear message
                    }
                    if (DrawButton(submitButton, "Create Account")) {
                        GuiCreateAccount(); // Call logic function
                    }
                    DrawText(message, startX, submitButton.y + 70, 20, (Color){ 220, 50, 50, 255 }); // Draw message (red)

                } break;

                case STAFF_DEPOSIT: // Deposit form
                {
                    DrawText("Deposit Funds", startX, startY - 70, 40, (Color){ 50, 50, 50, 255 }); // Title
                    DrawTextBox(&depositAccNoBox, "Account Number");
                    DrawTextBox(&depositAmountBox, "Amount to Deposit");

                    if (DrawButton(backButton, "Back")) {
                        currentScreen = STAFF_MAIN; // Go to main menu
                        strcpy(message, "");        // Clear message
                    }
                    if (DrawButton(submitButton, "Deposit")) {
                        GuiDeposit(); // Call logic function
                    }
                    DrawText(message, startX, submitButton.y + 70, 20, (Color){ 220, 50, 50, 255 }); // Draw message
                } break;

                case STAFF_WITHDRAW: // Withdraw form
                {
                    DrawText("Withdraw Funds", startX, startY - 70, 40, (Color){ 50, 50, 50, 255 }); // Title
                    DrawTextBox(&withdrawAccNoBox, "Account Number");
                    DrawTextBox(&withdrawAmountBox, "Amount to Withdraw");

                    if (DrawButton(backButton, "Back")) {
                        currentScreen = STAFF_MAIN; // Go to main menu
                        strcpy(message, "");        // Clear message
                    }
                    if (DrawButton(submitButton, "Withdraw")) {
                        GuiWithdraw(); // Call logic function
                    }
                    DrawText(message, startX, submitButton.y + 70, 20, (Color){ 220, 50, 50, 255 }); // Draw message
                } break;
                
                case STAFF_FORGOT_PIN: // Reset PIN form
                {
                    DrawText("Reset User PIN", startX, startY - 70, 40, (Color){ 50, 50, 50, 255 }); // Title
                    DrawTextBox(&forgotPinAccNoBox, "Account Number");
                    DrawTextBox(&forgotPinNewPinBox, "New 4-Digit PIN");
                    
                    if (DrawButton(backButton, "Back")) {
                        currentScreen = STAFF_MAIN; // Go to main menu
                        strcpy(message, "");        // Clear message
                    }
                    if (DrawButton(submitButton, "Set New PIN")) {
                        GuiForgotPin(); // Call logic function
                    }
                    DrawText(message, startX, submitButton.y + 70, 20, (Color){ 220, 50, 50, 255 }); // Draw message
                } break;

                case STAFF_VIEW_PENDING: // View Pending Complaints screen
                {
                    DrawComplaintsScreen(true); // Call drawing function (true for pending)
                    if (DrawButton(backButtonFull, "Back")) {
                        currentScreen = STAFF_MAIN; // Go to main menu
                        strcpy(message, "");        // Clear message
                    }
                } break;

                case STAFF_VIEW_RESOLVED: // View Resolved Complaints screen
                {
                    DrawComplaintsScreen(false); // Call drawing function (false for resolved)
                    if (DrawButton(backButtonFull, "Back")) {
                        currentScreen = STAFF_MAIN; // Go to main menu
                        strcpy(message, "");        // Clear message
                    }
                } break;
                
                case STAFF_RESPOND: // Respond to Complaint form
                {
                    DrawText("Respond to Complaint", startX, startY - 70, 40, (Color){ 50, 50, 50, 255 }); // Title
                    DrawTextBox(&respondIdBox, "Complaint ID");
                    DrawTextBox(&respondResponseBox, "Staff Response (max 500 chars)"); // Large text box

                    if (DrawButton(backRespondButton, "Back")) { // Use special layout buttons
                        currentScreen = STAFF_MAIN; // Go to main menu
                        strcpy(message, "");        // Clear message
                    }
                    if (DrawButton(submitRespondButton, "Submit Response")) { // Use special layout buttons
                        GuiRespondToComplaint(); // Call logic function
                    }
                    DrawText(message, startX, submitRespondButton.y + 70, 20, (Color){ 220, 50, 50, 255 }); // Draw message
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
void InitTextBox(TextBox *box, Rectangle bounds, bool isPassword)
{
    box->bounds = bounds;                         // Set position and size
    memset(box->text, 0, MAX_INPUT_CHARS + 1);    // Clear the text buffer
    box->charCount = 0;                           // Reset character count
    box->active = false;                          // Set to inactive
    box->isPassword = isPassword;                 // Set password flag
}

// Draws a button and returns true if clicked
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

// Draws the text box on screen
void DrawTextBox(TextBox *box, const char *placeholder)
{
    DrawRectangleRec(box->bounds, WHITE); // Draw white background

    if (box->active)
    {
        DrawRectangleLinesEx(box->bounds, 2, (Color){ 80, 120, 200, 255 }); // Thick blue border if active
    }
    else
    {
        DrawRectangleLinesEx(box->bounds, 1, GRAY); // Thin gray border if inactive
    }

    if (box->charCount > 0) // If there is text
    {
        if (box->isPassword) // If it's a password
        {
            char passwordStars[MAX_INPUT_CHARS + 1] = { 0 }; // Buffer for stars
            for(int i = 0; i < box->charCount; i++) passwordStars[i] = '*'; // Fill with stars
            DrawText(passwordStars, box->bounds.x + 10, box->bounds.y + (box->bounds.height - 20) / 2, 20, BLACK); // Draw stars
        }
        else
        {
            DrawText(box->text, box->bounds.x + 10, box->bounds.y + (box->bounds.height - 20) / 2, 20, BLACK); // Draw the real text
        }
    }
    else
    {
        DrawText(placeholder, box->bounds.x + 10, box->bounds.y + (box->bounds.height - 20) / 2, 20, LIGHTGRAY); // Draw placeholder if empty
    }

    // --- Draw Blinking Cursor ---
    if (box->active)
    {
        if (((int)(GetTime() * 2.0f)) % 2 == 0) // Blink logic
        {
            float textWidth;
            if (box->isPassword)
            {
                char passwordStars[MAX_INPUT_CHARS + 1] = { 0 };
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
}

// Handles keyboard input for an active text box
void UpdateTextBox(TextBox *box)
{
    if (box->active) // Only run if box is active
    {
        // Special case for multi-line text box (response box)
        if (box->bounds.height > 60 && IsKeyPressed(KEY_ENTER))
        {
             if (box->charCount < MAX_INPUT_CHARS) 
             {
                box->text[box->charCount] = '\n'; // Add newline character
                box->charCount++;
             }
        }
        
        int key = GetCharPressed(); // Get character (Unicode)
        while (key > 0) // Process all keys pressed this frame
        {
            // Only accept standard printable ASCII and if not full
            if ((key >= 32 && key <= 126) && (box->charCount < MAX_INPUT_CHARS))
            {
                box->text[box->charCount] = (char)key; // Add char to buffer
                box->text[box->charCount + 1] = '\0';  // Add null terminator
                box->charCount++;                      // Increment count
            }
            key = GetCharPressed(); // Get next key in queue
        }

        // Handle backspace (with repeat for holding down)
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyReleased(KEY_BACKSPACE) || IsKeyDown(KEY_BACKSPACE))
        {
            if (box->charCount > 0)
            {
                static double lastBackspaceTime = 0; // Timer for key repeat
                double time = GetTime();
                // Check if pressed, or if held down long enough for repeat
                if (IsKeyPressed(KEY_BACKSPACE) || (IsKeyDown(KEY_BACKSPACE) && (time - lastBackspaceTime > 0.05)))
                {
                    lastBackspaceTime = time; // Reset timer
                    box->charCount--;         // Decrement count
                    box->text[box->charCount] = '\0'; // Move null terminator back
                }
                else if (IsKeyReleased(KEY_BACKSPACE))
                {
                    lastBackspaceTime = 0; // Reset timer on release
                }
            }
        }
    }
}


// Deactivates all text boxes, then activates the one that was clicked (if any)
void ActivateClickedTextBox(void)
{
    // Deactivate all
    createNameBox.active = false;
    createPinBox.active = false;
    createDepositBox.active = false;
    depositAccNoBox.active = false;
    depositAmountBox.active = false;
    withdrawAccNoBox.active = false;
    withdrawAmountBox.active = false;
    forgotPinAccNoBox.active = false;
    forgotPinNewPinBox.active = false;
    respondIdBox.active = false;
    respondResponseBox.active = false;
    
    // Activate based on current screen and mouse position
    switch (currentScreen)
    {
        case STAFF_CREATE:
            if (CheckCollisionPointRec(mousePos, createNameBox.bounds)) createNameBox.active = true;
            if (CheckCollisionPointRec(mousePos, createPinBox.bounds)) createPinBox.active = true;
            if (CheckCollisionPointRec(mousePos, createDepositBox.bounds)) createDepositBox.active = true;
            break;
        case STAFF_DEPOSIT:
            if (CheckCollisionPointRec(mousePos, depositAccNoBox.bounds)) depositAccNoBox.active = true;
            if (CheckCollisionPointRec(mousePos, depositAmountBox.bounds)) depositAmountBox.active = true;
            break;
        case STAFF_WITHDRAW:
            if (CheckCollisionPointRec(mousePos, withdrawAccNoBox.bounds)) withdrawAccNoBox.active = true;
            if (CheckCollisionPointRec(mousePos, withdrawAmountBox.bounds)) withdrawAmountBox.active = true;
            break;
        case STAFF_FORGOT_PIN:
            if (CheckCollisionPointRec(mousePos, forgotPinAccNoBox.bounds)) forgotPinAccNoBox.active = true;
            if (CheckCollisionPointRec(mousePos, forgotPinNewPinBox.bounds)) forgotPinNewPinBox.active = true;
            break;
        case STAFF_RESPOND:
            if (CheckCollisionPointRec(mousePos, respondIdBox.bounds)) respondIdBox.active = true;
            if (CheckCollisionPointRec(mousePos, respondResponseBox.bounds)) respondResponseBox.active = true;
            break;
        default: break; // No text boxes on other screens
    }
}

// Resets all text boxes to their initial empty state
void ClearAllTextBoxes(void)
{
    InitTextBox(&createNameBox, createNameBox.bounds, createNameBox.isPassword);
    InitTextBox(&createPinBox, createPinBox.bounds, createPinBox.isPassword);
    InitTextBox(&createDepositBox, createDepositBox.bounds, createDepositBox.isPassword);
    InitTextBox(&depositAccNoBox, depositAccNoBox.bounds, depositAccNoBox.isPassword);
    InitTextBox(&depositAmountBox, depositAmountBox.bounds, depositAmountBox.isPassword);
    InitTextBox(&withdrawAccNoBox, withdrawAccNoBox.bounds, withdrawAccNoBox.isPassword);
    InitTextBox(&withdrawAmountBox, withdrawAmountBox.bounds, withdrawAmountBox.isPassword);
    InitTextBox(&forgotPinAccNoBox, forgotPinAccNoBox.bounds, forgotPinAccNoBox.isPassword);
    InitTextBox(&forgotPinNewPinBox, forgotPinNewPinBox.bounds, forgotPinNewPinBox.isPassword);
    InitTextBox(&respondIdBox, respondIdBox.bounds, respondIdBox.isPassword);
    InitTextBox(&respondResponseBox, respondResponseBox.bounds, respondResponseBox.isPassword);
}

//----------------------------------------------------------------------------------
// Module Functions Definition (Ported Backend Logic)
//----------------------------------------------------------------------------------

// Gets the current date/time as a formatted string
void getCurrentTimestamp(char *buffer, size_t size) {
    time_t now = time(NULL); // Get current time
    struct tm *tm_info = localtime(&now); // Convert to local time struct
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info); // Format as string
}

// Finds the next available account number (max + 1)
int nextAccountNumber(void) {
    FILE *fp = fopen(FILE_NAME, "r"); // Open accounts.csv
    if (!fp) return 1001; // If file doesn't exist, start at 1001
    int max = 1000;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) { // Read line by line
        char copy[1024];
        strcpy(copy, line);
        char *tok = strtok(copy, ","); // Get first token (account number)
        if (!tok) continue;
        int id = atoi(tok);
        if (id > max) max = id; // Keep track of the highest ID
    }
    fclose(fp);
    return max + 1; // Return the next number
}

// Checks if a customer account exists in accounts.csv
int accountExists(int accNo) {
    FILE *fp = fopen(FILE_NAME, "r");
    if (!fp) return 0; // File not found
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char copy[1024];
        strcpy(copy, line);
        char *t = strtok(copy, ","); // Get first token
        if (!t) continue;
        if (atoi(t) == accNo) { fclose(fp); return 1; } // Found
    }
    fclose(fp);
    return 0; // Not found
}

// Reads a single customer account's details from accounts.csv
Account getAccount(int accNo) {
    FILE *fp = fopen(FILE_NAME, "r");
    Account acc;
    memset(&acc, 0, sizeof(acc)); // Zero out the struct
    if (!fp) return acc; // File not found, return empty struct
    char line[1024];
    while (fgets(line, sizeof(line), fp)) { // Read line by line
        char copy[1024];
        strcpy(copy, line);
        char *t = strtok(copy, ","); // Get first token (ID)
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
            acc.accountNumber = id;
            break; // Stop searching
        }
    }
    fclose(fp);
    return acc; // Return the populated (or empty) struct
}

// Rewrites a single customer account's details in accounts.csv
void updateAccount(Account acc) {
    FILE *fp = fopen(FILE_NAME, "r"); // Open original for reading
    if (!fp) return;
    FILE *temp = fopen("temp.csv", "w"); // Open temp for writing
    if (!temp) { fclose(fp); return; }

    char line[1024];
    while (fgets(line, sizeof(line), fp)) { // Read original line by line
        char copy[1024];
        strcpy(copy, line);
        char *tok = strtok(copy, ","); // Get first token (ID)
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

// Appends a transaction to transactions.csv
void logTransaction(int accNo, const char *type, double amount, double balance) {
    FILE *fp = fopen(TRANSACTION_FILE, "a"); // Open for appending
    if (!fp) return; // File error
    char timeStr[64];
    getCurrentTimestamp(timeStr, sizeof(timeStr)); // Get timestamp
    // Write transaction as a new CSV line
    fprintf(fp, "%d,%s,%.2f,%.2f,%s\n", accNo, type, amount, balance, timeStr);
    fclose(fp);
}

// Appends a staff action to changelog.csv
void logChange(const char *staff, const char *action, int accNo, double amount) {
    FILE *fp = fopen(CHANGELOG_FILE, "a"); // Open for appending
    if (!fp) return; // File error
    char timeStr[64];
    getCurrentTimestamp(timeStr, sizeof(timeStr)); // Get timestamp
    // Write log as a new CSV line
    fprintf(fp, "%s,%s,%d,%.2f,%s\n", staff, action, accNo, amount, timeStr);
    fclose(fp);
}

// Generates a random 16-digit card number (starting with '5')
void generateCardNumber(char *cardNumber) {
    cardNumber[0] = '5'; // Standard prefix
    for (int i = 1; i < 16; i++)
        cardNumber[i] = '0' + rand() % 10; // Random digit 0-9
    cardNumber[16] = '\0'; // Null terminator
}

// Finds the next available complaint ID (max + 1)
int getNextComplaintId(void) {
    FILE *fp = fopen(COMPLAINT_FILE, "r"); // Open complaints.csv
    if (!fp) return 1; // If file doesn't exist, start at 1

    int maxId = 0;
    char line[2048];
    while (fgets(line, sizeof(line), fp)) { // Read line by line
        int id;
        if (sscanf(line, "%d,", &id) == 1 && id > maxId) // Parse just the first integer (ID)
            maxId = id; // Keep track of the highest ID
    }
    fclose(fp);
    return maxId + 1; // Return the next number
}


// --- ADDED: Functions to sync with users.txt ---

// Checks if a username (account number) already exists in users.txt
static int usernameExistsInUserFile(const char* username) {
    FILE *fp = fopen(USER_FILE, "r"); // Open users.txt
    if (!fp) return 0; // File not found
    
    char fileUser[MAX_LOGIN_CHARS + 1];
    char filePass[MAX_LOGIN_CHARS + 1];
    double bal;
    int blocked;
    
    // Read 4-column format
    while (fscanf(fp, "%s %s %lf %d", fileUser, filePass, &bal, &blocked) != EOF) {
        if (strcmp(fileUser, username) == 0) { // If username (account number) matches
            fclose(fp);
            return 1; // Found
        }
    }
    fclose(fp);
    return 0; // Not found
}

// Gets a single user account's details from users.txt
static UserFileAccount GetUserFileAccount(const char* username) {
    FILE *file = fopen(USER_FILE, "r");
    UserFileAccount acc;
    memset(&acc, 0, sizeof(acc)); // Zero out the struct
    if (file == NULL) return acc; // Return empty struct if file not found

    int isBlocked;
    // Read 4-column format
    while (fscanf(file, "%s %s %lf %d", acc.username, acc.password, &acc.balance, &isBlocked) != EOF)
    {
        if (strcmp(username, acc.username) == 0) // If this is the user
        {
            acc.isCardBlocked = isBlocked; // Set block status
            fclose(file);
            return acc; // Found it, return populated struct
        }
    }
    fclose(file);
    memset(&acc, 0, sizeof(acc)); // Not found, return empty struct
    return acc;
}

// Rewrites the users.txt file with updated data for one account
static bool UpdateUserFile(UserFileAccount updatedAccount) {
    UserFileAccount accounts[MAX_ACCOUNTS]; // In-memory array
    int count = 0;
    FILE *file = fopen(USER_FILE, "r"); // Open users.txt for reading
    if (file == NULL) return false;

    // Read all accounts from users.txt into the array
    while (fscanf(file, "%s %s %lf %d", accounts[count].username, accounts[count].password, &accounts[count].balance, &accounts[count].isCardBlocked) != EOF)
    {
        // Find the matching account and update it in the array
        if (strcmp(accounts[count].username, updatedAccount.username) == 0)
        {
            accounts[count] = updatedAccount;
        }
        count++;
        if (count >= MAX_ACCOUNTS) break; // Stop if array is full
    }
    fclose(file);

    // Write all accounts from the array back to the file
    file = fopen(USER_FILE, "w"); // Open for writing (truncates)
    if (file == NULL) return false;
    for (int i = 0; i < count; i++)
    {
        fprintf(file, "%s %s %.2f %d\n", 
                accounts[i].username, 
                accounts[i].password, 
                accounts[i].balance, 
                accounts[i].isCardBlocked);
    }
    fclose(file);
    return true; // Success
}


//----------------------------------------------------------------------------------
// Module Functions Definition (New GUI-driven Logic)
//----------------------------------------------------------------------------------

// Logic handler for the "Create Account" button
void GuiCreateAccount(void) {
    // 1. Validate Input
    if (createNameBox.charCount == 0) {
        strcpy(message, "Error: Name cannot be empty.");
        return;
    }
    // Check if PIN is exactly 4 characters and all are digits
    if (strlen(createPinBox.text) != 4 || strspn(createPinBox.text, "0123456789") != 4) {
        strcpy(message, "Error: PIN must be 4 digits.");
        return;
    }
    double amount = atof(createDepositBox.text); // Convert text to double
    if (amount < 0) {
        strcpy(message, "Error: Initial deposit cannot be negative.");
        return;
    }

    // 2. Open accounts.csv for appending
    FILE *fp = fopen(FILE_NAME, "a");
    if (!fp) {
        strcpy(message, "Error: Could not open accounts.csv.");
        return;
    }

    // 3. Process Data
    Account acc; // Create new account struct
    acc.accountNumber = nextAccountNumber(); // Get next available ID
    strncpy(acc.name, createNameBox.text, sizeof(acc.name) - 1); // Copy name
    acc.name[sizeof(acc.name) - 1] = '\0'; // Ensure null termination
    strcpy(acc.pin, createPinBox.text); // Copy PIN
    acc.balance = amount; // Set balance
    generateCardNumber(acc.cardNumber); // Generate random card number
    acc.cardBlocked = 0; // Default to active
    strncpy(acc.createdBy, loggedInStaff, sizeof(acc.createdBy) - 1); // Set creator
    acc.createdBy[sizeof(acc.createdBy) - 1] = '\0'; // Ensure null termination

    // 4. Write to accounts.csv
    fprintf(fp, "%d,%s,%s,%.2f,%d,%s,%s\n",
            acc.accountNumber, acc.name, acc.pin, acc.balance,
            acc.cardBlocked, acc.cardNumber, acc.createdBy);
    fclose(fp);
    
    // --- ADDED: Write to users.txt for login sync ---
    char accNumStr[20];
    sprintf(accNumStr, "%d", acc.accountNumber); // Convert account number to string

    // Check if account number (as username) already exists in users.txt
    if (usernameExistsInUserFile(accNumStr)) {
        // This is a safety check; should not happen if nextAccountNumber is correct
        logChange(loggedInStaff, "Create Account FAILED (user.txt conflict)", acc.accountNumber, 0);
        strcpy(message, "Error: Account number conflict in users.txt.");
        return; // Stop
    }

    FILE *userFp = fopen(USER_FILE, "a"); // Open users.txt for appending
    if (!userFp) {
        strcpy(message, "Error: Could not open users.txt to create login.");
        // This is a critical error state: account exists in one file but not the other
        return;
    }
    
    // Write in the 4-column format: username(accNo) password(PIN) balance blocked
    fprintf(userFp, "%s %s %.2f %d\n", accNumStr, acc.pin, acc.balance, acc.cardBlocked);
    fclose(userFp);
    // --- End of ADDED section ---

    // 5. Log actions
    logTransaction(acc.accountNumber, "Account Created", acc.balance, acc.balance); // Log transaction
    logChange(acc.createdBy, "Create Account", acc.accountNumber, acc.balance); // Log staff action

    // 6. Set feedback
    sprintf(message, "Success! Account %d created for %s.", acc.accountNumber, acc.name);
    ClearAllTextBoxes(); // Reset form
}

// Logic handler for the "Deposit" button
void GuiDeposit(void) {
    // 1. Validate Input
    int accNo = atoi(depositAccNoBox.text);
    double amount = atof(depositAmountBox.text);

    if (accNo <= 0 || !accountExists(accNo)) { // Check if account exists
        strcpy(message, "Error: Account not found in accounts.csv.");
        return;
    }
    if (amount <= 0) {
        strcpy(message, "Error: Deposit amount must be positive.");
        return;
    }

    // 2. Process Data (accounts.csv)
    Account acc = getAccount(accNo); // Get current account data
    acc.balance += amount; // Add deposit
    updateAccount(acc); // Write updated data back to accounts.csv
    
    // --- ADDED: Sync with users.txt ---
    char accNumStr[20];
    sprintf(accNumStr, "%d", accNo);
    UserFileAccount userAcc = GetUserFileAccount(accNumStr); // Get user data from users.txt
    if (strlen(userAcc.username) > 0) { // Check if found
        userAcc.balance = acc.balance; // Sync balance
        UpdateUserFile(userAcc); // Write updated data back to users.txt
    }
    // --- End of ADDED section ---

    // 3. Log actions
    logTransaction(acc.accountNumber, "Deposit", amount, acc.balance); // Log transaction
    logChange(loggedInStaff, "Deposit", acc.accountNumber, amount); // Log staff action

    // 4. Set feedback
    sprintf(message, "Success! %.2f deposited. New balance: %.2f", amount, acc.balance);
    ClearAllTextBoxes(); // Reset form
}

// Logic handler for the "Withdraw" button
void GuiWithdraw(void) {
    // 1. Validate Input
    int accNo = atoi(withdrawAccNoBox.text);
    double amount = atof(withdrawAmountBox.text);

    if (accNo <= 0 || !accountExists(accNo)) { // Check if account exists
        strcpy(message, "Error: Account not found in accounts.csv.");
        return;
    }
    if (amount <= 0) {
        strcpy(message, "Error: Withdraw amount must be positive.");
        return;
    }

    // 2. Process Data (accounts.csv)
    Account acc = getAccount(accNo); // Get current account data
    if (acc.cardBlocked) { // Check block status
        strcpy(message, "Error: Card is blocked. Cannot withdraw.");
        return;
    }
    if (amount > acc.balance) { // Check for sufficient funds
        strcpy(message, "Error: Insufficient funds.");
        return;
    }

    acc.balance -= amount; // Subtract withdrawal
    updateAccount(acc); // Write updated data back to accounts.csv
    
    // --- ADDED: Sync with users.txt ---
    char accNumStr[20];
    sprintf(accNumStr, "%d", accNo);
    UserFileAccount userAcc = GetUserFileAccount(accNumStr); // Get user data from users.txt
    if (strlen(userAcc.username) > 0) { // Check if found
        userAcc.balance = acc.balance; // Sync balance
        UpdateUserFile(userAcc); // Write updated data back to users.txt
    }
    // --- End of ADDED section ---

    // 3. Log actions
    logTransaction(acc.accountNumber, "Withdraw", amount, acc.balance); // Log transaction
    logChange(loggedInStaff, "Withdraw", acc.accountNumber, amount); // Log staff action

    // 4. Set feedback
    sprintf(message, "Success! %.2f withdrawn. New balance: %.2f", amount, acc.balance);
    ClearAllTextBoxes(); // Reset form
}

// Logic handler for the "Reset PIN" button
void GuiForgotPin(void) {
    // 1. Validate Input
    int accNo = atoi(forgotPinAccNoBox.text);
    if (accNo <= 0 || !accountExists(accNo)) { // Check if account exists
        strcpy(message, "Error: Account not found in accounts.csv.");
        return;
    }
    // Check if new PIN is exactly 4 digits
    if (strlen(forgotPinNewPinBox.text) != 4 || strspn(forgotPinNewPinBox.text, "0123456789") != 4) {
        strcpy(message, "Error: New PIN must be 4 digits.");
        return;
    }

    // 2. Process Data (accounts.csv)
    Account acc = getAccount(accNo); // Get current account data
    strcpy(acc.pin, forgotPinNewPinBox.text); // Set new PIN
    updateAccount(acc); // Write updated data back to accounts.csv
    
    // --- ADDED: Sync with users.txt ---
    char accNumStr[20];
    sprintf(accNumStr, "%d", accNo);
    UserFileAccount userAcc = GetUserFileAccount(accNumStr); // Get user data from users.txt
    if (strlen(userAcc.username) > 0) { // Check if found
        strcpy(userAcc.password, acc.pin); // Sync PIN
        UpdateUserFile(userAcc); // Write updated data back to users.txt
    }
    // --- End of ADDED section ---
    
    // 3. Log action
    logChange(loggedInStaff, "PIN Reset", acc.accountNumber, 0.0); // Log staff action

    // 4. Set feedback
    sprintf(message, "Success! PIN for account %d has been reset.", acc.accountNumber);
    ClearAllTextBoxes(); // Reset form
}


// Loads all complaints from complaints.csv into the global in-memory arrays
void LoadComplaints(void) {
    FILE *fp = fopen(COMPLAINT_FILE, "r"); // Open complaints.csv
    if (!fp) {
        pendingCount = 0; // File not found, set counts to 0
        resolvedCount = 0;
        return;
    }

    pendingCount = 0; // Reset counts
    resolvedCount = 0;
    char line[2048]; // Buffer for one line

    // Read line by line
    while (fgets(line, sizeof(line), fp) && (pendingCount + resolvedCount < MAX_COMPLAINTS)) {
        Complaint c; // Struct to parse into
        // Parse the CSV line (>= 6 fields expected, response/timestamp might be empty initially)
        if (sscanf(line, "%d,%d,%49[^,],%499[^,],%19[^,],%499[^,],%63[^\n]",
                   &c.complaintId, &c.accountNumber, c.category,
                   c.description, c.status, c.response, c.timestamp) >= 6)
        {
            // Sort into appropriate array based on status
            if (strcmp(c.status, "Pending") == 0 || strcmp(c.status, "In Progress") == 0) {
                if (pendingCount < MAX_COMPLAINTS) {
                    pendingComplaints[pendingCount++] = c; // Add to pending array
                }
            } else if (strcmp(c.status, "Resolved") == 0) {
                if (resolvedCount < MAX_COMPLAINTS) {
                    resolvedComplaints[resolvedCount++] = c; // Add to resolved array
                }
            }
        }
    }
    fclose(fp);
}

// Draws the scrolling list UI for complaints
void DrawComplaintsScreen(bool drawPending) {
    // Set variables based on whether we're drawing pending or resolved
    const char* title = drawPending ? "Pending Complaints" : "Resolved Complaints";
    Complaint* list = drawPending ? pendingComplaints : resolvedComplaints;
    int count = drawPending ? pendingCount : resolvedCount;
    Vector2* scroll = drawPending ? &scrollPending : &scrollResolved;
    
    // --- Define layout ---
    float startX = 100;
    float startY = 120;
    float width = SCREEN_WIDTH - 200;
    float height = 500; // Height of the scroll panel

    DrawText(title, (SCREEN_WIDTH - MeasureText(title, 40)) / 2, startY, 40, (Color){ 50, 50, 50, 255 }); // Title

    Rectangle panelBounds = { startX, startY + 60, width, height }; // The main panel
    DrawRectangleRec(panelBounds, WHITE); // Panel background
    DrawRectangleLinesEx(panelBounds, 1, GRAY); // Panel border

    // Calculate total height of all content
    float contentHeight = 20; // Initial padding
    for (int i = 0; i < count; i++) {
        contentHeight += 160; // Each complaint item is 140px + 20px padding
    }
    
    BeginScissorMode(panelBounds.x, panelBounds.y, panelBounds.width, panelBounds.height); // Start scrollable area
    
    float currentY = panelBounds.y + scroll->y + 20; // Starting Y, adjusted by scroll
    
    if (count == 0) { // If no complaints
        DrawText("No complaints found.", panelBounds.x + 20, currentY, 20, GRAY);
    }

    // Loop through all complaints in the list
    for (int i = 0; i < count; i++) {
        Complaint c = list[i];
        
        // Culling: If item is off-screen, skip drawing its text (but still calc Y)
        if (currentY < panelBounds.y - 160 || currentY > panelBounds.y + panelBounds.height) {
            currentY += 160; // Move to next item's Y position
            continue;
        }

        // Draw item box
        DrawRectangle(panelBounds.x + 10, currentY, panelBounds.width - 20, 140, (Color){ 250, 250, 250, 255 }); // Light bg
        DrawRectangleLines(panelBounds.x + 10, currentY, panelBounds.width - 20, 140, LIGHTGRAY); // Border
        
        // Draw complaint details
        char buffer[1024];
        sprintf(buffer, "ID: %d | Account: %d | Category: %s | Date: %s",
                c.complaintId, c.accountNumber, c.category, c.timestamp);
        DrawText(buffer, panelBounds.x + 20, currentY + 10, 20, (Color){ 60, 90, 150, 255 }); // Header in blue
        
        sprintf(buffer, "Issue: %s", c.description);
        DrawText(buffer, panelBounds.x + 20, currentY + 40, 20, BLACK); // Description
        
        sprintf(buffer, "Status: %s", c.status);
        DrawText(buffer, panelBounds.x + 20, currentY + 70, 20, (Color){ 80, 80, 80, 255 }); // Status

        sprintf(buffer, "Response: %s", c.response);
        DrawText(buffer, panelBounds.x + 20, currentY + 100, 20, (Color){ 50, 150, 50, 255 }); // Response in green

        currentY += 160; // Move to next item's Y position
    }
    
    EndScissorMode(); // End scrollable area
    
    // --- Draw Scrollbar if content is taller than panel ---
    if (contentHeight > panelBounds.height) {
        float scrollbarWidth = 10;
        float scrollbarX = panelBounds.x + panelBounds.width - scrollbarWidth - 2;
        // Calculate scrollbar handle height (proportional to content visible)
        float scrollbarHeight = (panelBounds.height / contentHeight) * panelBounds.height;
        // Calculate scrollbar handle Y position
        float scrollbarY = panelBounds.y + (-scroll->y / (contentHeight - panelBounds.height)) * (panelBounds.height - scrollbarHeight) + 1;
        
        DrawRectangle(scrollbarX, panelBounds.y + 1, scrollbarWidth, panelBounds.height - 2, LIGHTGRAY); // Scrollbar track
        DrawRectangle(scrollbarX, scrollbarY, scrollbarWidth, scrollbarHeight, GRAY); // Scrollbar handle
    }
}

// Logic handler for the "Respond" button
void GuiRespondToComplaint(void) {
    // 1. Validate Input
    int compId = atoi(respondIdBox.text); // Get ID from text box
    if (compId <= 0) {
        strcpy(message, "Error: Invalid Complaint ID.");
        return;
    }
    if (respondResponseBox.charCount == 0) { // Check for empty response
        strcpy(message, "Error: Response cannot be empty.");
        return;
    }

    // 2. Open Files
    FILE *fp = fopen(COMPLAINT_FILE, "r"); // Open original for reading
    FILE *temp = fopen("temp_complaints.csv", "w"); // Open temp for writing
    if (!fp || !temp) {
        strcpy(message, "Error: Could not open complaints file.");
        if (fp) fclose(fp);
        if (temp) fclose(temp);
        return;
    }

    // 3. Process Data: Find and replace
    char line[2048]; // Line buffer
    int found = 0; // Flag
    while (fgets(line, sizeof(line), fp)) { // Read original line by line
        Complaint c;
        // Try to parse the line
        if (sscanf(line, "%d,%d,%49[^,],%499[^,],%19[^,],%499[^,],%63[^\n]",
                   &c.complaintId, &c.accountNumber, c.category,
                   c.description, c.status, c.response, c.timestamp) >= 6) {
            if (c.complaintId == compId) { // If this is the complaint to update
                found = 1;
                strcpy(c.status, "Resolved"); // Set status to Resolved
                strncpy(c.response, respondResponseBox.text, sizeof(c.response) - 1); // Copy response
                c.response[sizeof(c.response) - 1] = '\0'; // Ensure null termination
                
                // Sanitize response: remove newlines and commas to prevent breaking CSV
                for(int i = 0; c.response[i]; i++) {
                    if (c.response[i] == '\n' || c.response[i] == ',') c.response[i] = ' ';
                }
                
                // Write updated line to temp file
                fprintf(temp, "%d,%d,%s,%s,%s,%s,%s\n", c.complaintId, c.accountNumber,
                        c.category, c.description, c.status, c.response, c.timestamp);
            } else {
                fputs(line, temp); // Not the target, write original line
            }
        } else {
            fputs(line, temp); // Unparsed line, write as-is
        }
    }
    fclose(fp); // Close original
    fclose(temp); // Close temp

    // 4. Replace file and set feedback
    if (found) {
        remove(COMPLAINT_FILE); // Delete original
        rename("temp_complaints.csv", COMPLAINT_FILE); // Rename temp to original
        sprintf(message, "Success! Response for complaint %d submitted.", compId);
        logChange(loggedInStaff, "Responded to Complaint", compId, 0.0); // Log staff action
        ClearAllTextBoxes(); // Reset form
    } else {
        remove("temp_complaints.csv"); // Delete temp file
        strcpy(message, "Error: Complaint ID not found or already resolved.");
    }
}