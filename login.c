/*******************************************************************************************
*
* Banking System Login (Raylib)
*
* This program demonstrates a simple login/register system using Raylib for the GUI.
* It saves and retrieves user data from plain text files.
*
* This modified version launches:
* - 'userinterface.exe' upon successful user login.
* - 'staff_menu.exe' upon successful staff login.
* - 'admin_interface.exe' upon successful admin login.
*
* Compile example (Windows):
* gcc login.c -o login.exe -lraylib -lgdi32 -lwinmm -Wl,-subsystem,windows
*
********************************************************************************************/

#include "raylib.h"     // Include the core Raylib library for GUI, drawing, and input
#include <stdio.h>      // Include standard input/output for file operations (fopen, printf, etc.)
#include <string.h>     // Include string library for string manipulation (strcpy, strcmp, etc.)
#include <stdlib.h>     // Include standard library for system() function (to run external .exe)

//----------------------------------------------------------------------------------
// Defines and Types
//----------------------------------------------------------------------------------
#define SCREEN_WIDTH 1620   // Define the width of the application window
#define SCREEN_HEIGHT 920   // Define the height of the application window

#define MAX_INPUT_CHARS 50  // Define the maximum number of characters for input fields

// File names for storing credentials
const char *USER_FILE = "users.txt";    // File to store customer account data (account_num, pin, balance, blocked_status)
const char *STAFF_FILE = "staff.txt";   // File to store staff credentials (staff_id, password)
const char *ADMIN_FILE = "admin.txt";   // File to store admin credentials (admin_id, password)

// Different screens/states of the application
typedef enum {
    MAIN_MENU,          // The initial screen with login options
    USER_LOGIN,         // The screen for customer login
    STAFF_LOGIN,        // The screen for staff login
    ADMIN_LOGIN,        // The screen for admin login
    USER_DASHBOARD,     // Placeholder (now unused, as it launches an external app)
    STAFF_DASHBOARD,    // Placeholder (now unused, as it launches an external app)
    ADMIN_DASHBOARD     // Placeholder (now unused, as it launches an external app)
} AppScreen;

// Simple struct to hold user credentials
typedef struct {
    char username[MAX_INPUT_CHARS + 1]; // Buffer to store the username (or account/staff ID)
    char password[MAX_INPUT_CHARS + 1]; // Buffer to store the password (or PIN)
} User;

// Helper struct for managing text input fields
typedef struct {
    Rectangle bounds;                   // The position and size (x, y, width, height) of the text box
    char text[MAX_INPUT_CHARS + 1];     // The actual text content of the box
    int charCount;                      // The current number of characters in the text buffer
    bool active;                        // Flag to check if the text box is currently selected (active)
    bool isPassword;                    // Flag to determine if text should be drawn as asterisks (*)
} TextBox;

//----------------------------------------------------------------------------------
// Global Variables
//----------------------------------------------------------------------------------
static AppScreen currentScreen = MAIN_MENU;     // Variable to track the current active screen, starts at MAIN_MENU
static Vector2 mousePos = { 0.0f, 0.0f };       // A 2D vector to store the mouse's current (x, y) position
static char message[100] = { 0 };               // A string to hold feedback messages (e.g., "Login Successful", "Error...")
static char loggedInUser[MAX_INPUT_CHARS + 1] = { 0 }; // Stores the username upon successful login (used for placeholder dashboards)

// Define Textboxes for various screens
static TextBox userLoginUsernameBox;    // Text box for the user's account number
static TextBox userLoginPasswordBox;    // Text box for the user's PIN
static TextBox staffLoginUsernameBox;   // Text box for the staff's ID
static TextBox staffLoginPasswordBox;   // Text box for the staff's password
static TextBox adminLoginUsernameBox;   // Text box for the admin's ID
static TextBox adminLoginPasswordBox;   // Text box for the admin's password

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void InitTextBox(TextBox *box, Rectangle bounds, bool isPassword); // Function to initialize/reset a text box
static bool DrawButton(Rectangle bounds, const char *text);               // Function to draw a clickable button and check if it's pressed
static void DrawTextBox(TextBox *box, const char *placeholder);           // Function to draw a text box on the screen
static void UpdateTextBox(TextBox *box);                                  // Function to handle keyboard input for an active text box
static bool SaveUserToFile(User user, const char *filename);              // Function to save new user credentials to a file
static bool CheckLogin(User user, const char *filename);                  // Function to validate credentials against a file
static void ClearAllTextBoxes();                                          // Function to reset all text boxes to empty

//----------------------------------------------------------------------------------
// Main Entry Point
//----------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Banking System - Login"); // Create the application window

    // Center layout calculations
    float boxWidth = 400;                                           // Desired width for text boxes and buttons
    float boxHeight = 50;                                           // Desired height for text boxes
    float spacing = 20;                                             // Vertical spacing between elements
    float startX = (SCREEN_WIDTH - boxWidth) / 2;                   // Calculate the X position to center elements
    float startY = (SCREEN_HEIGHT - (boxHeight * 2 + spacing)) / 2 - 50; // Calculate the starting Y position for the first text box

    // Initialize all text boxes with their calculated positions
    InitTextBox(&userLoginUsernameBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false); // Not a password field
    InitTextBox(&userLoginPasswordBox, (Rectangle){ startX, startY + boxHeight + spacing, boxWidth, boxHeight }, true);  // Is a password field
    
    InitTextBox(&staffLoginUsernameBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false); // Not a password field
    InitTextBox(&staffLoginPasswordBox, (Rectangle){ startX, startY + boxHeight + spacing, boxWidth, boxHeight }, true);  // Is a password field

    InitTextBox(&adminLoginUsernameBox, (Rectangle){ startX, startY, boxWidth, boxHeight }, false); // Not a password field
    InitTextBox(&adminLoginPasswordBox, (Rectangle){ startX, startY + boxHeight + spacing, boxWidth, boxHeight }, true);  // Is a password field

    SetTargetFPS(60);   // Set the application to run at 60 frames per second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Loop continues as long as the window is open (no X pressed, no ESC)
    {
        // Update
        //----------------------------------------------------------------------------------
        mousePos = GetMousePosition(); // Get the current (x, y) coordinates of the mouse each frame
        
        // Handle activation/deactivation on mouse click
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) // Check if the left mouse button was just pressed
        {
            // Deactivate all boxes by default when a click occurs
            userLoginUsernameBox.active = false;
            userLoginPasswordBox.active = false;
            staffLoginUsernameBox.active = false;
            staffLoginPasswordBox.active = false;
            adminLoginUsernameBox.active = false;
            adminLoginPasswordBox.active = false;

            // Activate the specific box that was clicked, based on the current screen
            switch (currentScreen) // Check which screen is currently active
            {
                case USER_LOGIN: // If on the user login screen
                    if (CheckCollisionPointRec(mousePos, userLoginUsernameBox.bounds)) userLoginUsernameBox.active = true; // Activate username box if clicked
                    if (CheckCollisionPointRec(mousePos, userLoginPasswordBox.bounds)) userLoginPasswordBox.active = true; // Activate password box if clicked
                    break;
                case STAFF_LOGIN: // If on the staff login screen
                    if (CheckCollisionPointRec(mousePos, staffLoginUsernameBox.bounds)) staffLoginUsernameBox.active = true; // Activate username box if clicked
                    if (CheckCollisionPointRec(mousePos, staffLoginPasswordBox.bounds)) staffLoginPasswordBox.active = true; // Activate password box if clicked
                    break;
                case ADMIN_LOGIN: // If on the admin login screen
                    if (CheckCollisionPointRec(mousePos, adminLoginUsernameBox.bounds)) adminLoginUsernameBox.active = true; // Activate username box if clicked
                    if (CheckCollisionPointRec(mousePos, adminLoginPasswordBox.bounds)) adminLoginPasswordBox.active = true; // Activate password box if clicked
                    break;
                default: break; // Do nothing on other screens (like MAIN_MENU)
            }
        }

        // Update the text content for whichever box is active
        if (userLoginUsernameBox.active) UpdateTextBox(&userLoginUsernameBox); // Handle key input for this box
        if (userLoginPasswordBox.active) UpdateTextBox(&userLoginPasswordBox); // Handle key input for this box
        if (staffLoginUsernameBox.active) UpdateTextBox(&staffLoginUsernameBox); // Handle key input for this box
        if (staffLoginPasswordBox.active) UpdateTextBox(&staffLoginPasswordBox); // Handle key input for this box
        if (adminLoginUsernameBox.active) UpdateTextBox(&adminLoginUsernameBox); // Handle key input for this box
        if (adminLoginPasswordBox.active) UpdateTextBox(&adminLoginPasswordBox); // Handle key input for this box
        
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing(); // Start the drawing phase

            ClearBackground((Color){ 245, 245, 245, 255 }); // Clear the screen with a light gray background

            // Draw common header
            DrawRectangle(0, 0, SCREEN_WIDTH, 80, (Color){ 60, 90, 150, 255 }); // Draw the dark blue header bar
            DrawText("DAIICT Swiss Bank", 30, 20, 40, WHITE); // Draw the bank's title on the header

            // Button layout variables
            Rectangle button1 = { (SCREEN_WIDTH - 400) / 2, (SCREEN_HEIGHT / 2) - 120, 400, 80 }; // Position for the first main menu button
            Rectangle button2 = { (SCREEN_WIDTH - 400) / 2, (SCREEN_HEIGHT / 2), 400, 80 };       // Position for the second main menu button
            Rectangle button3 = { (SCREEN_WIDTH - 400) / 2, (SCREEN_HEIGHT / 2) + 120, 400, 80 }; // Position for the third main menu button
            Rectangle backButton = { 30, SCREEN_HEIGHT - 70, 150, 40 };                            // Position for the "Back" button
            Rectangle actionButton = { startX, startY + (boxHeight + spacing) * 2, boxWidth, 50 };  // Position for the "Login" button on login screens

            switch (currentScreen) // Draw content based on the current screen
            {
                case MAIN_MENU:
                {
                    DrawText("Main Menu", (SCREEN_WIDTH - MeasureText("Main Menu", 60)) / 2, 200, 60, (Color){ 50, 50, 50, 255 }); // Draw title

                    // Draw buttons and check if they are clicked
                    if (DrawButton(button1, "User Login")) currentScreen = USER_LOGIN;   // Go to USER_LOGIN if clicked
                    if (DrawButton(button2, "Staff Login")) currentScreen = STAFF_LOGIN; // Go to STAFF_LOGIN if clicked
                    if (DrawButton(button3, "Admin Login")) currentScreen = ADMIN_LOGIN; // Go to ADMIN_LOGIN if clicked
                } break;

                case USER_LOGIN:
                {
                    DrawText("User Login", startX, startY - 70, 40, (Color){ 50, 50, 50, 255 }); // Draw title
                    // --- MODIFICATION: Changed placeholder text ---
                    DrawTextBox(&userLoginUsernameBox, "Account Number"); // Draw the username box
                    DrawTextBox(&userLoginPasswordBox, "PIN");            // Draw the password box
                    
                    if (DrawButton(actionButton, "Login")) // Check if the Login button is clicked
                    {
                        User user; // Create a temporary user struct
                        strcpy(user.username, userLoginUsernameBox.text); // Copy text from username box
                        strcpy(user.password, userLoginPasswordBox.text); // Copy text from password box
                        
                        // User file is no longer created by default here, 
                        // it must be created by the staff interface.
                        
                        if (CheckLogin(user, USER_FILE)) // Validate credentials against users.txt
                        {
                            strcpy(message, "Login Successful! Launching dashboard..."); // Set success message
                            
                            char command[512]; // Buffer for the system command
                            // Pass the username (account number) to the external program
                            sprintf(command, "start userinterface.exe %s", user.username); // Build the command string
                            
                            system(command); // Run the external .exe file
                            
                            ClearAllTextBoxes(); // Clear input fields

                            // Close the login window
                            CloseWindow(); // Close this application
                            return 0;      // Exit the login program successfully
                        }
                        else
                        {
                            strcpy(message, "Error: Invalid Account Number or PIN."); // Set error message
                        }
                    }
                    
                    if (DrawButton(backButton, "Back to Menu")) // Check if Back button is clicked
                    {
                        currentScreen = MAIN_MENU; // Go back to the main menu
                        strcpy(message, "");       // Clear any error messages
                        ClearAllTextBoxes();       // Clear input fields
                    }

                    DrawText(message, startX, actionButton.y + 70, 20, (Color){ 220, 50, 50, 255 }); // Draw the message (error or success)
                } break;

                case STAFF_LOGIN:
                {
                    DrawText("Staff Login", startX, startY - 70, 40, (Color){ 50, 50, 50, 255 }); // Draw title
                    DrawTextBox(&staffLoginUsernameBox, "Staff ID"); // Draw the username box
                    DrawTextBox(&staffLoginPasswordBox, "Password"); // Draw the password box
                    
                    if (DrawButton(actionButton, "Login")) // Check if Login button is clicked
                    {
                        User user; // Create a temporary user struct
                        strcpy(user.username, staffLoginUsernameBox.text); // Copy text from username box
                        strcpy(user.password, staffLoginPasswordBox.text); // Copy text from password box
                        
                        FILE *file = fopen(STAFF_FILE, "r"); // Try to open the staff file for reading
                        if (file == NULL) // If the file doesn't exist
                        {
                            User defaultStaff = { "staff01", "pass123" }; // Create a default staff user
                            SaveUserToFile(defaultStaff, STAFF_FILE);    // Save the default user to create the file
                        }
                        else
                        {
                            fclose(file); // File exists, so just close it
                        }

                        if (CheckLogin(user, STAFF_FILE)) // Validate credentials against staff.txt
                        {
                            strcpy(message, "Login Successful! Launching staff menu..."); // Set success message
                            
                            char command[512]; // Buffer for the system command
                            sprintf(command, "start staffinterface.exe %s 2810", user.username); // Build the command string
                            
                            system(command); // Run the external .exe file
                            
                            ClearAllTextBoxes(); // Clear input fields

                            CloseWindow(); // Close this application
                            return 0;      // Exit the login program
                        }
                        else
                        {
                            strcpy(message, "Error: Invalid Staff ID or password."); // Set error message
                        }
                    }
                    
                    if (DrawButton(backButton, "Back to Menu")) // Check if Back button is clicked
                    {
                        currentScreen = MAIN_MENU; // Go back to the main menu
                        strcpy(message, "");       // Clear any error messages
                        ClearAllTextBoxes();       // Clear input fields
                    }

                    DrawText(message, startX, actionButton.y + 70, 20, (Color){ 220, 50, 50, 255 }); // Draw the message (error or success)
                } break;

                case ADMIN_LOGIN:
                {
                    DrawText("Admin Login", startX, startY - 70, 40, (Color){ 50, 50, 50, 255 }); // Draw title
                    DrawTextBox(&adminLoginUsernameBox, "Admin ID"); // Draw the username box
                    DrawTextBox(&adminLoginPasswordBox, "Password"); // Draw the password box
                    
                    if (DrawButton(actionButton, "Login")) // Check if Login button is clicked
                    {
                        User user; // Create a temporary user struct
                        strcpy(user.username, adminLoginUsernameBox.text); // Copy text from username box
                        strcpy(user.password, adminLoginPasswordBox.text); // Copy text from password box

                        FILE *file = fopen(ADMIN_FILE, "r"); // Try to open the admin file for reading
                        if (file == NULL) // If the file doesn't exist
                        {
                            User defaultAdmin = { "admin", "adminpass" }; // Create a default admin user
                            SaveUserToFile(defaultAdmin, ADMIN_FILE);    // Save the default user to create the file
                        }
                        else
                        {
                            fclose(file); // File exists, so just close it
                        }
                        
                        if (CheckLogin(user, ADMIN_FILE)) // Validate credentials against admin.txt
                        { 
                             strcpy(message, "Login Successful! Launching dashboard..."); // Set success message
                            
                            char command[512]; // Buffer for the system command
                            sprintf(command, "start admin_interface.exe %s 2810", user.username); // Build the command string
                            
                            system(command); // Run the external .exe file
                            
                            ClearAllTextBoxes(); // Clear input fields

                            CloseWindow(); // Close this application
                            return 0;      // Exit the login program
                        
                        }
                        else
                        {
                            strcpy(message, "Error: Invalid Admin ID or password."); // Set error message
                        }
                    }
                    
                    if (DrawButton(backButton, "Back to Menu")) // Check if Back button is clicked
                    {
                        currentScreen = MAIN_MENU; // Go back to the main menu
                        strcpy(message, "");       // Clear any error messages
                        ClearAllTextBoxes();       // Clear input fields
                    }

                    DrawText(message, startX, actionButton.y + 70, 20, (Color){ 220, 50, 50, 255 }); // Draw the message (error or success)
                } break;
                
                // --- Placeholder Logged-in Screens (Unchanged, currently unreachable) ---
                // These screens are no longer used because the program exits on successful login.
                
                case USER_DASHBOARD:
                {
                    char welcomeText[100];
                    sprintf(welcomeText, "Welcome, %s!", loggedInUser); // Create welcome message
                    DrawText(welcomeText, (SCREEN_WIDTH - MeasureText(welcomeText, 60)) / 2, 300, 60, (Color){ 50, 50, 50, 255 });
                    DrawText("This is your user dashboard.", (SCREEN_WIDTH - MeasureText("This is your user dashboard.", 30)) / 2, 400, 30, (Color){ 80, 80, 80, 255 });
                    
                    if (DrawButton(button2, "Logout")) // Check for logout
                    {
                        currentScreen = MAIN_MENU; // Go to main menu
                        strcpy(message, "");       // Clear message
                        strcpy(loggedInUser, "");  // Clear logged in user
                    }
                } break;

                case STAFF_DASHBOARD:
                {
                    char welcomeText[100];
                    sprintf(welcomeText, "Welcome, Staff Member %s!", loggedInUser); // Create welcome message
                    DrawText(welcomeText, (SCREEN_WIDTH - MeasureText(welcomeText, 60)) / 2, 300, 60, (Color){ 50, 50, 50, 255 });
                    DrawText("This is your staff dashboard.", (SCREEN_WIDTH - MeasureText("This is your staff dashboard.", 30)) / 2, 400, 30, (Color){ 80, 80, 80, 255 });

                    if (DrawButton(button2, "Logout")) // Check for logout
                    {
                        currentScreen = MAIN_MENU; // Go to main menu
                        strcpy(message, "");       // Clear message
                        strcpy(loggedInUser, "");  // Clear logged in user
                    }
                } break;
                
                case ADMIN_DASHBOARD:
                {
                    char welcomeText[100];
                    sprintf(welcomeText, "Welcome, Admin %s!", loggedInUser); // Create welcome message
                    DrawText(welcomeText, (SCREEN_WIDTH - MeasureText(welcomeText, 60)) / 2, 300, 60, (Color){ 50, 50, 50, 255 });
                    DrawText("This is your admin dashboard.", (SCREEN_WIDTH - MeasureText("This is your admin dashboard.", 30)) / 2, 400, 30, (Color){ 80, 80, 80, 255 });
                    
                    if (DrawButton(button2, "Logout")) // Check for logout
                    {
                        currentScreen = MAIN_MENU; // Go to main menu
                        strcpy(message, "");       // Clear message
                        strcpy(loggedInUser, "");  // Clear logged in user
                    }
                } break;
            }

        EndDrawing(); // Finish the drawing phase
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and free all resources (OpenGL context)
    //--------------------------------------------------------------------------------------

    return 0; // Exit the program
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

// Initialize a text box
void InitTextBox(TextBox *box, Rectangle bounds, bool isPassword)
{
    box->bounds = bounds;                         // Set the position and size
    memset(box->text, 0, MAX_INPUT_CHARS + 1);    // Clear the text buffer (fill with 0s)
    box->charCount = 0;                           // Reset the character count
    box->active = false;                          // Set the box to inactive
    box->isPassword = isPassword;                 // Set whether it's a password field
}

// Draw a button and return true if clicked
bool DrawButton(Rectangle bounds, const char *text)
{
    bool clicked = false;                               // Flag to return, default to false
    Color bgColor = (Color){ 80, 120, 200, 255 };       // Normal blue color
    Color fgColor = WHITE;                              // Text color
    
    if (CheckCollisionPointRec(mousePos, bounds))       // Check if the mouse is hovering over the button
    {
        bgColor = (Color){ 100, 150, 230, 255 };        // Change to a lighter blue on hover
        
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))    // Check if the button is clicked
        {
            clicked = true;                             // Set the clicked flag
            bgColor = (Color){ 60, 90, 150, 255 };      // Change to a darker blue on click
        }
    }
    
    DrawRectangleRounded(bounds, 0.2f, 4, bgColor);     // Draw the button's background rectangle with rounded corners
    
    int textWidth = MeasureText(text, 20);              // Measure the width of the text to center it
    DrawText(text, bounds.x + (bounds.width - textWidth) / 2, bounds.y + (bounds.height - 20) / 2, 20, fgColor); // Draw the text centered
    
    return clicked; // Return true if clicked, false otherwise
}

// Draw a text box
void DrawTextBox(TextBox *box, const char *placeholder)
{
    DrawRectangleRec(box->bounds, WHITE); // Draw the white background of the text box
    
    if (box->active) // If the box is active (selected)
    {
        DrawRectangleLinesEx(box->bounds, 2, (Color){ 80, 120, 200, 255 }); // Draw a thick blue border
    }
    else
    {
        DrawRectangleLinesEx(box->bounds, 1, GRAY); // Draw a thin gray border
    }
    
    if (box->charCount > 0) // If there is text in the box
    {
        if (box->isPassword) // If it's a password field
        {
            char passwordStars[MAX_INPUT_CHARS + 1] = { 0 }; // Create a temporary string for asterisks
            for(int i = 0; i < box->charCount; i++) passwordStars[i] = '*'; // Fill it with asterisks
            DrawText(passwordStars, box->bounds.x + 10, box->bounds.y + (box->bounds.height - 20) / 2, 20, BLACK); // Draw the asterisks
        }
        else // If it's a normal text field
        {
            DrawText(box->text, box->bounds.x + 10, box->bounds.y + (box->bounds.height - 20) / 2, 20, BLACK); // Draw the actual text
        }
    }
    else // If the box is empty
    {
        DrawText(placeholder, box->bounds.x + 10, box->bounds.y + (box->bounds.height - 20) / 2, 20, LIGHTGRAY); // Draw the placeholder text
    }
    
    if (box->active) // If the box is active
    {
        // This logic creates a blinking cursor
        if (((int)(GetTime() * 2.0f)) % 2 == 0) // Blink on/off based on time
        {
            float textWidth;
            if (box->isPassword) // Need to measure the width of the asterisks
            {
                char passwordStars[MAX_INPUT_CHARS + 1] = { 0 };
                for(int i = 0; i < box->charCount; i++) passwordStars[i] = '*';
                textWidth = MeasureText(passwordStars, 20); // Measure asterisk string
            }
            else
            {
                textWidth = MeasureText(box->text, 20); // Measure the actual text
            }
            DrawRectangle(box->bounds.x + 10 + textWidth, box->bounds.y + 10, 2, box->bounds.height - 20, (Color){ 50, 50, 50, 255 }); // Draw the cursor
        }
    }
}

// Update text box logic (handle key presses)
void UpdateTextBox(TextBox *box)
{
    if (box->active) // Only update if the box is active
    {
        int key = GetCharPressed(); // Get the next character pressed (Unicode)
        
        while (key > 0) // Loop through all characters pressed this frame
        {
            // Check if the key is a printable character (ASCII 32-126) and we haven't reached the max length
            if ((key >= 32) && (box->charCount < MAX_INPUT_CHARS))
            {
                box->text[box->charCount] = (char)key; // Add the character to the text buffer
                box->text[box->charCount + 1] = '\0';  // Add the null terminator
                box->charCount++;                      // Increment the character count
            }
            key = GetCharPressed(); // Get the next character in the queue
        }
        
        if (IsKeyPressed(KEY_BACKSPACE)) // Check if backspace is pressed
        {
            if (box->charCount > 0) // If there are characters to delete
            {
                box->charCount--;                     // Decrement the character count
                box->text[box->charCount] = '\0';     // Move the null terminator back
            }
        }
    }
}

// Save a user to a file (appends)
bool SaveUserToFile(User user, const char *filename)
{
    FILE *file = fopen(filename, "a"); // Open the file in "append" mode (adds to the end)
    if (file == NULL) // If the file couldn't be opened
    {
        printf("Error: Could not open file %s for writing.\n", filename);
        return false; // Return failure
    }
    
    // --- MODIFICATION: Save in the 4-column format expected by userinterface.c ---
    // This function is only used for default staff/admin, so balance/blocked is 0.
    
    // Check if the file is NOT users.txt (i.e., it's staff.txt or admin.txt)
    if (strcmp(filename, USER_FILE) != 0)
    {
        fprintf(file, "%s %s\n", user.username, user.password); // Save in 2-column format
    }
    else
    {
        // This else block is for users.txt, which needs 4 columns.
        // However, this function is only called for STAFF_FILE and ADMIN_FILE now.
        // For safety, we'll keep the old logic for non-user files.
        // If this function were to be used for users.txt, it would save with default 0.00 balance and 0 blocked status.
        fprintf(file, "%s %s 0.00 0\n", user.username, user.password); // Save in 4-column format
    }
    
    fclose(file); // Close the file
    return true;  // Return success
}

// Check if a user's credentials are valid
bool CheckLogin(User user, const char *filename)
{
    FILE *file = fopen(filename, "r"); // Open the file in "read" mode
    if (file == NULL) // If the file couldn't be opened (e.g., doesn't exist)
    {
        printf("Error: Could not open file %s for reading.\n", filename);
        return false; // Return failure (can't log in if file doesn't exist)
    }
    
    char fileUsername[MAX_INPUT_CHARS + 1]; // Buffer to read username from file
    char filePassword[MAX_INPUT_CHARS + 1]; // Buffer to read password from file
    bool found = false;                     // Flag to track if a match is found
    
    // --- MODIFICATION: Handle both 2-column and 4-column files ---
    if (strcmp(filename, USER_FILE) == 0) // If we are checking the users.txt file
    {
        // users.txt has 4 columns: user pass balance blocked
        double balance; // Temporary variable to read balance (unused here)
        int isBlocked;  // Temporary variable to read blocked status (unused here)
        
        // Read file line by line, matching the 4-column format
        while (fscanf(file, "%s %s %lf %d", fileUsername, filePassword, &balance, &isBlocked) != EOF)
        {
            // Compare the input user with the user read from the file
            if (strcmp(user.username, fileUsername) == 0 && strcmp(user.password, filePassword) == 0)
            {
                found = true; // Match found
                break;        // Stop searching
            }
        }
    }
    else // If we are checking staff.txt or admin.txt
    {
        // staff.txt and admin.txt have 2 columns: user pass
        // Read file line by line, matching the 2-column format
        while (fscanf(file, "%s %s", fileUsername, filePassword) != EOF)
        {
            // Compare the input user with the user read from the file
            if (strcmp(user.username, fileUsername) == 0 && strcmp(user.password, filePassword) == 0)
            {
                found = true; // Match found
                break;        // Stop searching
            }
        }
    }
    
    fclose(file); // Close the file
    return found; // Return true if found, false otherwise
}

// Helper to clear all text boxes, e.g., on screen change
void ClearAllTextBoxes()
{
    // Call InitTextBox for every text box to reset it to its default empty state
    InitTextBox(&userLoginUsernameBox, userLoginUsernameBox.bounds, userLoginUsernameBox.isPassword);
    InitTextBox(&userLoginPasswordBox, userLoginPasswordBox.bounds, userLoginPasswordBox.isPassword);
    InitTextBox(&staffLoginUsernameBox, staffLoginUsernameBox.bounds, staffLoginUsernameBox.isPassword);
    InitTextBox(&staffLoginPasswordBox, staffLoginPasswordBox.bounds, staffLoginPasswordBox.isPassword);
    InitTextBox(&adminLoginUsernameBox, adminLoginUsernameBox.bounds, adminLoginUsernameBox.isPassword);
    InitTextBox(&adminLoginPasswordBox, adminLoginPasswordBox.bounds, adminLoginPasswordBox.isPassword);
}