# Pildhora App

Pildhora is a comprehensive smart pillbox management system designed to bridge the gap between elderly patients and their caregivers. It combines a user-friendly mobile application with IoT hardware to ensure medication adherence and peace of mind.

## 📱 Overview

The platform consists of two distinct interfaces within a single application:

*   **Patient App:** A simplified, high-contrast, and accessible interface for elderly users. It focuses on clear reminders, one-tap intake confirmation, and seamless connectivity with the physical pillbox.
*   **Caregiver App:** A robust dashboard for family members or healthcare providers. It allows for full medication management, real-time monitoring, adherence tracking, and device configuration.

## ✨ Key Features

### For Patients
*   **Smart Reminders:** Timely notifications for medication intakes.
*   **One-Tap Actions:** Easy confirmation of taken doses.
*   **Visual Clarity:** Large text, high contrast, and intuitive icons.
*   **Device Status:** Real-time battery and connectivity indicators for the Pildhora Pillbox.

### For Caregivers
*   **Remote Management:** Add, edit, or remove medications remotely.
*   **Real-Time Adherence:** Instant updates when a patient takes (or misses) a dose.
*   **AI Insights:** Powered by Google Vertex AI (Gemini) to analyze adherence patterns and generate reports.
*   **Multi-Patient Support:** Manage multiple patients from a single dashboard.
*   **Inventory Tracking:** Automatic tracking of pill quantities with low-stock alerts.

## 🛠️ Tech Stack

*   **Framework:** React Native (Expo)
*   **Styling:** NativeWind (Tailwind CSS) & Custom Design System
*   **Backend:** Firebase (Auth, Firestore, Realtime Database, Functions)
*   **AI Integration:** Google Vertex AI (Gemini)
*   **Hardware Integration:** BLE & Wi-Fi (ESP8266 based)
*   **State Management:** Redux Toolkit
*   **Navigation:** Expo Router

## 🚀 Getting Started

### Prerequisites
*   Node.js (LTS recommended)
*   Expo CLI
*   iOS Simulator (Mac) or Android Emulator

### Installation

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/username/pildhora-app.git
    cd pildhora-app
    ```

2.  **Install dependencies:**
    ```bash
    npm install
    ```

3.  **Environment Configuration:**
    Create a `.env` file in the root directory based on `.env.example`. You will need your Firebase configuration keys.
    ```env
    EXPO_PUBLIC_FIREBASE_API_KEY=your_api_key
    EXPO_PUBLIC_FIREBASE_AUTH_DOMAIN=your_project.firebaseapp.com
    ...
    ```

### Running the App

*   **Start the development server:**
    ```bash
    npm start
    ```
*   **Run on Android:**
    ```bash
    npm run android
    ```
*   **Run on iOS:**
    ```bash
    npm run ios
    ```

## 📁 Project Structure

*   `app/`: Application screens and routing (Expo Router).
    *   `caregiver/`: Screens specific to the caregiver flow.
    *   `patient/`: Screens specific to the patient flow.
*   `src/`: Core source code.
    *   `components/`: Reusable UI components.
    *   `hooks/`: Custom React hooks.
    *   `services/`: API and hardware integration services.
    *   `store/`: Redux state management.
    *   `theme/`: Design tokens and styling constants.

## 🤝 Contributing

Contributions are welcome! Please read our contributing guidelines before submitting a pull request.

## 📄 License

This project is licensed under the MIT License.
