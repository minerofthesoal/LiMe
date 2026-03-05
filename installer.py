#!/usr/bin/env python3
"""
LiMe OS Graphical Installer
Main installer application with Qt-based UI
"""

import sys
import os
import json
import subprocess
import threading
from pathlib import Path
from enum import Enum

# Try importing Qt5, fall back to PyQt5
try:
    from PyQt5.QtWidgets import (
        QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
        QLabel, QPushButton, QLineEdit, QComboBox, QCheckBox, QProgressBar,
        QTextEdit, QStackedWidget, QFileDialog, QMessageBox, QListWidget,
        QListWidgetItem, QSpinBox, QFormLayout, QDialog, QGroupBox,
        QScrollArea
    )
    from PyQt5.QtCore import Qt, pyqtSignal, QThread, QObject
    from PyQt5.QtGui import QFont, QPixmap, QIcon
except ImportError:
    print("Error: PyQt5 is required. Install with: pip install PyQt5")
    sys.exit(1)


class InstallStep(Enum):
    WELCOME = 0
    DISK_SELECTION = 1
    PARTITION = 2
    LOCALE = 3
    KEYBOARD = 4
    USER = 5
    PACKAGES = 6
    AI_MODELS = 7
    REVIEW = 8
    INSTALL = 9
    COMPLETE = 10


class InstallationThread(QThread):
    """Background thread for actual installation"""
    progress = pyqtSignal(int)
    status = pyqtSignal(str)
    error = pyqtSignal(str)
    finished = pyqtSignal()

    def __init__(self, config):
        super().__init__()
        self.config = config
        self.is_running = True

    def run(self):
        """Execute installation"""
        try:
            self.status.emit("Starting LiMe OS installation...")
            self.progress.emit(5)

            # Step 1: Partition disk
            self.status.emit(f"Partitioning disk: {self.config['disk']}")
            self._partition_disk()
            self.progress.emit(20)

            # Step 2: Install base system
            self.status.emit("Installing Arch Linux base system...")
            self._install_base()
            self.progress.emit(40)

            # Step 3: Install LiMe DE
            self.status.emit("Installing LiMe Desktop Environment...")
            self._install_lime_de()
            self.progress.emit(60)

            # Step 4: Create user
            self.status.emit(f"Creating user: {self.config['username']}")
            self._create_user()
            self.progress.emit(75)

            # Step 5: Configure system
            self.status.emit("Configuring system...")
            self._configure_system()
            self.progress.emit(85)

            # Step 6: Install AI models
            if self.config.get('install_ai_models'):
                self.status.emit("Installing AI models...")
                self._install_ai_models()
            self.progress.emit(95)

            # Step 7: Finalize
            self.status.emit("Finalizing installation...")
            self._finalize()
            self.progress.emit(100)

            self.status.emit("Installation complete!")
            self.finished.emit()

        except Exception as e:
            self.error.emit(str(e))

    def _partition_disk(self):
        """Partition the selected disk"""
        # This is a placeholder - actual implementation would use parted/fdisk
        if hasattr(self, 'config') and 'disk' in self.config:
            # Example: real implementation would partition the disk
            pass

    def _install_base(self):
        """Install Arch Linux base system"""
        # Placeholder for base system installation
        # Real implementation: pacstrap, fstab generation, etc.
        pass

    def _install_lime_de(self):
        """Install LiMe Desktop Environment"""
        # Placeholder for DE installation
        pass

    def _create_user(self):
        """Create user account"""
        # Placeholder for user creation
        pass

    def _configure_system(self):
        """Configure system settings"""
        # Placeholder for system configuration
        pass

    def _install_ai_models(self):
        """Download and install AI models"""
        # Placeholder for AI model installation
        pass

    def _finalize(self):
        """Finalize installation"""
        # Placeholder for finalization
        pass


class LiMeInstaller(QMainWindow):
    """Main installer window"""

    def __init__(self):
        super().__init__()
        self.current_step = InstallStep.WELCOME
        self.config = {}
        self.init_ui()

    def init_ui(self):
        """Initialize user interface"""
        self.setWindowTitle("LiMe OS Installer")
        self.setGeometry(100, 100, 900, 600)

        # Main widget
        main_widget = QWidget()
        self.setCentralWidget(main_widget)

        # Layout
        layout = QVBoxLayout()

        # Title
        title = QLabel("LiMe OS Installer")
        title_font = QFont()
        title_font.setPointSize(18)
        title_font.setBold(True)
        title.setFont(title_font)
        layout.addWidget(title)

        # Stacked widget for pages
        self.pages = QStackedWidget()
        layout.addWidget(self.pages)

        # Create pages
        self.create_welcome_page()
        self.create_disk_selection_page()
        self.create_locale_page()
        self.create_user_page()
        self.create_packages_page()
        self.create_ai_models_page()
        self.create_review_page()
        self.create_install_page()
        self.create_complete_page()

        # Navigation buttons
        nav_layout = QHBoxLayout()
        self.prev_btn = QPushButton("Previous")
        self.prev_btn.clicked.connect(self.prev_page)
        self.next_btn = QPushButton("Next")
        self.next_btn.clicked.connect(self.next_page)
        self.cancel_btn = QPushButton("Cancel")
        self.cancel_btn.clicked.connect(self.cancel_install)

        nav_layout.addWidget(self.prev_btn)
        nav_layout.addStretch()
        nav_layout.addWidget(self.next_btn)
        nav_layout.addWidget(self.cancel_btn)

        layout.addLayout(nav_layout)
        main_widget.setLayout(layout)

        self.show_page(InstallStep.WELCOME)

    def create_welcome_page(self):
        """Welcome page"""
        widget = QWidget()
        layout = QVBoxLayout()

        title = QLabel("Welcome to LiMe OS")
        title_font = QFont()
        title_font.setPointSize(16)
        title.setFont(title_font)
        layout.addWidget(title)

        desc = QLabel(
            "This installer will guide you through setting up LiMe OS.\n\n"
            "LiMe OS is a modern, customizable Linux distribution based on Arch Linux\n"
            "with an integrated AI assistant and custom desktop environment.\n\n"
            "Please ensure you have backed up your data before proceeding."
        )
        layout.addWidget(desc)

        layout.addStretch()

        self.pages.addWidget(widget)

    def create_disk_selection_page(self):
        """Disk selection page"""
        widget = QWidget()
        layout = QVBoxLayout()

        label = QLabel("Select Installation Disk")
        font = QFont()
        font.setPointSize(12)
        label.setFont(font)
        layout.addWidget(label)

        layout.addWidget(QLabel("Available disks:"))
        self.disk_list = QListWidget()
        # Populate with available disks
        layout.addWidget(self.disk_list)

        layout.addStretch()

        self.pages.addWidget(widget)

    def create_locale_page(self):
        """Locale/language page"""
        widget = QWidget()
        layout = QVBoxLayout()

        layout.addWidget(QLabel("System Language & Locale"))

        form_layout = QFormLayout()

        self.locale_combo = QComboBox()
        self.locale_combo.addItems([
            "English (US)",
            "English (UK)",
            "Spanish",
            "French",
            "German",
            "Chinese (Simplified)",
            "Japanese"
        ])
        form_layout.addRow("Language:", self.locale_combo)

        self.timezone_combo = QComboBox()
        self.timezone_combo.addItems([
            "UTC",
            "America/New_York",
            "Europe/London",
            "Europe/Paris",
            "Asia/Tokyo",
            "Asia/Shanghai"
        ])
        form_layout.addRow("Timezone:", self.timezone_combo)

        layout.addLayout(form_layout)
        layout.addStretch()

        self.pages.addWidget(widget)

    def create_user_page(self):
        """User account creation page"""
        widget = QWidget()
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Create User Account"))

        form_layout = QFormLayout()

        self.username_input = QLineEdit()
        self.username_input.setPlaceholderText("Lowercase username")
        form_layout.addRow("Username:", self.username_input)

        self.password_input = QLineEdit()
        self.password_input.setEchoMode(QLineEdit.Password)
        form_layout.addRow("Password:", self.password_input)

        self.password_confirm = QLineEdit()
        self.password_confirm.setEchoMode(QLineEdit.Password)
        form_layout.addRow("Confirm Password:", self.password_confirm)

        self.hostname_input = QLineEdit()
        self.hostname_input.setPlaceholderText("Computer hostname")
        self.hostname_input.setText("lime-machine")
        form_layout.addRow("Hostname:", self.hostname_input)

        layout.addLayout(form_layout)
        layout.addStretch()

        self.pages.addWidget(widget)

    def create_packages_page(self):
        """Additional packages selection page"""
        widget = QWidget()
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Select Additional Software"))

        group = QGroupBox("Development Tools")
        group_layout = QVBoxLayout()
        group_layout.addWidget(QCheckBox("GCC, Make, CMake"))
        group_layout.addWidget(QCheckBox("Git & Version Control"))
        group_layout.addWidget(QCheckBox("Docker"))
        group_layout.addWidget(QCheckBox("VS Code"))
        group.setLayout(group_layout)
        layout.addWidget(group)

        group2 = QGroupBox("Office & Productivity")
        group2_layout = QVBoxLayout()
        group2_layout.addWidget(QCheckBox("LibreOffice"))
        group2_layout.addWidget(QCheckBox("Thunderbird (Email)"))
        group2_layout.addWidget(QCheckBox("GIMP (Image Editor)"))
        group2.setLayout(group2_layout)
        layout.addWidget(group2)

        layout.addStretch()

        self.pages.addWidget(widget)

    def create_ai_models_page(self):
        """AI models selection page"""
        widget = QWidget()
        layout = QVBoxLayout()

        layout.addWidget(QLabel("LiMe AI - LLM Model Selection"))

        desc = QLabel(
            "Select which AI models to pre-install.\n"
            "Models can also be installed later from the LiMe AI app.\n"
            "Each model requires 2-20GB of disk space."
        )
        layout.addWidget(desc)

        group = QGroupBox("Qwen Models (Alibaba)")
        group_layout = QVBoxLayout()
        self.qwen_checkbox = QCheckBox("Qwen 2B (Small, ~2GB)")
        group_layout.addWidget(self.qwen_checkbox)
        group_layout.addWidget(QCheckBox("Qwen 7B (Medium, ~7GB)"))
        group_layout.addWidget(QCheckBox("Qwen 14B (Large, ~14GB)"))
        group.setLayout(group_layout)
        layout.addWidget(group)

        group2 = QGroupBox("Nix-AI Models")
        group2_layout = QVBoxLayout()
        group2_layout.addWidget(QCheckBox("Nix Small"))
        group2_layout.addWidget(QCheckBox("Nix Medium"))
        group2.setLayout(group2_layout)
        layout.addWidget(group2)

        layout.addStretch()

        self.pages.addWidget(widget)

    def create_review_page(self):
        """Review configuration page"""
        widget = QWidget()
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Review Installation Settings"))

        self.review_text = QTextEdit()
        self.review_text.setReadOnly(True)
        layout.addWidget(self.review_text)

        confirm = QLabel("Click 'Next' to begin installation")
        layout.addWidget(confirm)

        self.pages.addWidget(widget)

    def create_install_page(self):
        """Installation progress page"""
        widget = QWidget()
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Installing LiMe OS..."))

        self.install_status = QLabel("Preparing...")
        layout.addWidget(self.install_status)

        self.progress_bar = QProgressBar()
        layout.addWidget(self.progress_bar)

        self.install_log = QTextEdit()
        self.install_log.setReadOnly(True)
        layout.addWidget(self.install_log)

        self.pages.addWidget(widget)

    def create_complete_page(self):
        """Installation complete page"""
        widget = QWidget()
        layout = QVBoxLayout()

        title = QLabel("Installation Complete!")
        title_font = QFont()
        title_font.setPointSize(16)
        title.setFont(title_font)
        layout.addWidget(title)

        layout.addWidget(QLabel(
            "LiMe OS has been successfully installed!\n\n"
            "Your system is ready to use. You can now:\n"
            "• Explore the LiMe Desktop Environment\n"
            "• Configure your AI assistant\n"
            "• Install additional software from the app store\n\n"
            "Click 'Finish' to reboot into your new system."
        ))

        layout.addStretch()

        self.pages.addWidget(widget)

    def show_page(self, step):
        """Show a specific page"""
        self.current_step = step
        self.pages.setCurrentIndex(step.value)
        self.update_buttons()

    def update_buttons(self):
        """Update button states based on current page"""
        self.prev_btn.setEnabled(self.current_step.value > 0)
        self.next_btn.setText("Finish" if self.current_step == InstallStep.COMPLETE else "Next")

    def next_page(self):
        """Go to next page"""
        if self.current_step == InstallStep.REVIEW:
            self.start_installation()
        elif self.current_step == InstallStep.COMPLETE:
            self.reboot_system()
        else:
            next_step = InstallStep(min(self.current_step.value + 1, InstallStep.COMPLETE.value))
            self.show_page(next_step)

    def prev_page(self):
        """Go to previous page"""
        if self.current_step.value > 0:
            prev_step = InstallStep(self.current_step.value - 1)
            self.show_page(prev_step)

    def start_installation(self):
        """Start the actual installation"""
        self.show_page(InstallStep.INSTALL)

        # Collect configuration
        self.config = {
            'username': self.username_input.text(),
            'hostname': self.hostname_input.text(),
            'locale': self.locale_combo.currentText(),
            'timezone': self.timezone_combo.currentText(),
        }

        # Start installation thread
        self.install_thread = InstallationThread(self.config)
        self.install_thread.status.connect(self.update_status)
        self.install_thread.progress.connect(self.update_progress)
        self.install_thread.error.connect(self.handle_error)
        self.install_thread.finished.connect(self.installation_complete)
        self.install_thread.start()

    def update_status(self, status):
        """Update installation status"""
        self.install_status.setText(status)
        self.install_log.append(f"[*] {status}")

    def update_progress(self, value):
        """Update progress bar"""
        self.progress_bar.setValue(value)

    def handle_error(self, error):
        """Handle installation error"""
        QMessageBox.critical(self, "Installation Error", f"An error occurred:\n{error}")
        self.cancel_install()

    def installation_complete(self):
        """Installation finished successfully"""
        self.show_page(InstallStep.COMPLETE)

    def reboot_system(self):
        """Reboot the system"""
        reply = QMessageBox.question(
            self, "Reboot",
            "Reboot system now?",
            QMessageBox.Yes | QMessageBox.No
        )
        if reply == QMessageBox.Yes:
            os.system("sudo reboot")

    def cancel_install(self):
        """Cancel installation"""
        if self.current_step == InstallStep.INSTALL:
            reply = QMessageBox.question(
                self, "Cancel Installation",
                "Are you sure? This will stop the installation process.",
                QMessageBox.Yes | QMessageBox.No
            )
            if reply == QMessageBox.Yes:
                self.close()
        else:
            self.close()


def main():
    """Main entry point"""
    app = QApplication(sys.argv)
    installer = LiMeInstaller()
    sys.exit(app.exec_())


if __name__ == '__main__':
    main()
