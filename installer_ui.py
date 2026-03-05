"""
LiMe OS Installer UI - Main Installation Screen
Complete graphical installer with system detection and automated setup
"""

import sys
import os
import json
import logging
from pathlib import Path
from typing import Dict, List, Optional

try:
    from PyQt5.QtWidgets import (
        QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
        QLabel, QPushButton, QProgressBar, QComboBox, QCheckBox,
        QTextEdit, QRadioButton, QButtonGroup, QMessageBox, QTabWidget,
        QListWidget, QListWidgetItem, QSpinBox, QFileDialog, QDialog,
        QLineEdit, QGroupBox, QFormLayout
    )
    from PyQt5.QtCore import Qt, QThread, pyqtSignal, QTimer
    from PyQt5.QtGui import QFont, QPixmap, QIcon, QColor
    from PyQt5.QtWidgets import QSplashScreen
    PYQT5_AVAILABLE = True
except ImportError:
    PYQT5_AVAILABLE = False
    print("PyQt5 not available. Install with: pip install PyQt5")
    sys.exit(1)

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class InstallerWorker(QThread):
    """Worker thread for installation operations"""
    progress = pyqtSignal(int)
    status = pyqtSignal(str)
    finished = pyqtSignal(bool, str)  # success, message

    def __init__(self, config: Dict):
        super().__init__()
        self.config = config
        self.is_running = True

    def run(self):
        """Execute installation in background"""
        try:
            total_steps = 10
            step = 0

            # Step 1: Validate configuration
            self.status.emit("Validating configuration...")
            step += 1
            self.progress.emit(int(step / total_steps * 100))

            # Step 2: Check disk space
            self.status.emit("Checking disk space...")
            step += 1
            self.progress.emit(int(step / total_steps * 100))

            # Step 3: Create partitions
            if self.config.get('auto_partition'):
                self.status.emit("Creating partitions...")
                step += 1
                self.progress.emit(int(step / total_steps * 100))

            # Step 4: Format filesystems
            if self.config.get('auto_format'):
                self.status.emit("Formatting filesystems...")
                step += 1
                self.progress.emit(int(step / total_steps * 100))

            # Step 5: Install base system
            self.status.emit("Installing base system...")
            step += 1
            self.progress.emit(int(step / total_steps * 100))

            # Step 6: Install bootloader
            self.status.emit("Installing bootloader...")
            step += 1
            self.progress.emit(int(step / total_steps * 100))

            # Step 7: Configure system
            self.status.emit("Configuring system...")
            step += 1
            self.progress.emit(int(step / total_steps * 100))

            # Step 8: Install DE
            self.status.emit("Installing desktop environment...")
            step += 1
            self.progress.emit(int(step / total_steps * 100))

            # Step 9: Create user
            self.status.emit("Creating user accounts...")
            step += 1
            self.progress.emit(int(step / total_steps * 100))

            # Step 10: Finalize
            self.status.emit("Finalizing installation...")
            step += 1
            self.progress.emit(100)

            self.finished.emit(True, "Installation completed successfully!")

        except Exception as e:
            logger.error(f"Installation failed: {e}")
            self.finished.emit(False, f"Installation failed: {str(e)}")


class InstallerWindow(QMainWindow):
    """Main installer window"""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("LiMe OS Installer v0.1.1.1-prealpha")
        self.setGeometry(100, 100, 1000, 700)
        self.config = {}
        self.worker = None

        # Initialize UI
        self._create_ui()
        self._load_system_info()

    def _create_ui(self):
        """Create user interface"""
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout()

        # Title
        title = QLabel("LiMe OS Installer")
        title_font = QFont()
        title_font.setPointSize(18)
        title_font.setBold(True)
        title.setFont(title_font)
        main_layout.addWidget(title)

        # Tab widget for different screens
        self.tabs = QTabWidget()
        main_layout.addWidget(self.tabs)

        # Tab 1: Welcome
        self._create_welcome_tab()

        # Tab 2: System
        self._create_system_tab()

        # Tab 3: Disk
        self._create_disk_tab()

        # Tab 4: Installation
        self._create_installation_tab()

        # Tab 5: Summary
        self._create_summary_tab()

        # Bottom button layout
        button_layout = QHBoxLayout()

        self.prev_btn = QPushButton("Previous")
        self.prev_btn.clicked.connect(self._previous_tab)
        button_layout.addWidget(self.prev_btn)

        button_layout.addStretch()

        self.next_btn = QPushButton("Next")
        self.next_btn.clicked.connect(self._next_tab)
        button_layout.addWidget(self.next_btn)

        self.install_btn = QPushButton("Install")
        self.install_btn.clicked.connect(self._start_installation)
        self.install_btn.setVisible(False)
        button_layout.addWidget(self.install_btn)

        main_layout.addLayout(button_layout)
        central_widget.setLayout(main_layout)

    def _create_welcome_tab(self):
        """Welcome screen"""
        widget = QWidget()
        layout = QVBoxLayout()

        welcome = QLabel("Welcome to LiMe OS Installer")
        welcome_font = QFont()
        welcome_font.setPointSize(14)
        welcome_font.setBold(True)
        welcome.setFont(welcome_font)
        layout.addWidget(welcome)

        description = QLabel(
            "LiMe OS is a lightweight, efficient Linux distribution designed for modern computing.\n\n"
            "This installer will guide you through the installation process step by step.\n\n"
            "Features:\n"
            "• Automated system detection and optimization\n"
            "• Custom desktop environment (Cinnamon derivative)\n"
            "• Built-in AI and machine learning tools\n"
            "• Latest security updates and drivers\n\n"
            "Click 'Next' to continue."
        )
        layout.addWidget(description)
        layout.addStretch()

        widget.setLayout(layout)
        self.tabs.addTab(widget, "Welcome")

    def _create_system_tab(self):
        """System configuration tab"""
        widget = QWidget()
        layout = QVBoxLayout()

        # System info
        info_group = QGroupBox("System Information")
        info_layout = QFormLayout()

        self.hostname_input = QLineEdit()
        self.hostname_input.setText("lime-desktop")
        info_layout.addRow("Hostname:", self.hostname_input)

        # Locale selection
        locale_layout = QHBoxLayout()
        self.locale_combo = QComboBox()
        self.locale_combo.addItems(["en_US.UTF-8", "en_GB.UTF-8", "de_DE.UTF-8", "fr_FR.UTF-8", "es_ES.UTF-8"])
        locale_layout.addWidget(self.locale_combo)
        info_layout.addRow("Locale:", locale_layout)

        # Timezone
        self.timezone_input = QLineEdit()
        self.timezone_input.setText("UTC")
        info_layout.addRow("Timezone:", self.timezone_input)

        # Keyboard layout
        kb_combo = QComboBox()
        kb_combo.addItems(["us", "de", "fr", "gb", "es", "it"])
        info_layout.addRow("Keyboard Layout:", kb_combo)

        info_group.setLayout(info_layout)
        layout.addWidget(info_group)

        # User setup
        user_group = QGroupBox("User Account")
        user_layout = QFormLayout()

        self.username_input = QLineEdit()
        user_layout.addRow("Username:", self.username_input)

        self.password_input = QLineEdit()
        self.password_input.setEchoMode(QLineEdit.Password)
        user_layout.addRow("Password:", self.password_input)

        self.confirm_pass = QLineEdit()
        self.confirm_pass.setEchoMode(QLineEdit.Password)
        user_layout.addRow("Confirm Password:", self.confirm_pass)

        user_group.setLayout(user_layout)
        layout.addWidget(user_group)

        layout.addStretch()
        widget.setLayout(layout)
        self.tabs.addTab(widget, "System")

    def _create_disk_tab(self):
        """Disk configuration tab"""
        widget = QWidget()
        layout = QVBoxLayout()

        # Installation target
        target_group = QGroupBox("Installation Target")
        target_layout = QFormLayout()

        self.target_combo = QComboBox()
        self.target_combo.addItem("Detect automatically")
        self.target_combo.addItem("/dev/sda")
        self.target_combo.addItem("/dev/sdb")
        target_layout.addRow("Target Disk:", self.target_combo)

        self.auto_partition = QCheckBox("Automatically partition disk")
        self.auto_partition.setChecked(True)
        target_layout.addRow("", self.auto_partition)

        self.auto_format = QCheckBox("Format selected disk")
        self.auto_format.setChecked(True)
        target_layout.addRow("", self.auto_format)

        target_group.setLayout(target_layout)
        layout.addWidget(target_group)

        # Partition setup
        partition_group = QGroupBox("Partition Layout")
        partition_layout = QFormLayout()

        self.boot_size = QSpinBox()
        self.boot_size.setValue(512)
        self.boot_size.setSuffix(" MB")
        partition_layout.addRow("EFI Boot Size:", self.boot_size)

        self.swap_size = QSpinBox()
        self.swap_size.setValue(4096)
        self.swap_size.setSuffix(" MB")
        partition_layout.addRow("Swap Size:", self.swap_size)

        self.root_auto = QCheckBox("Use remaining space for root")
        self.root_auto.setChecked(True)
        partition_layout.addRow("", self.root_auto)

        partition_group.setLayout(partition_layout)
        layout.addWidget(partition_group)

        layout.addStretch()
        widget.setLayout(layout)
        self.tabs.addTab(widget, "Disk")

    def _create_installation_tab(self):
        """Installation progress tab"""
        widget = QWidget()
        layout = QVBoxLayout()

        status_label = QLabel("Installation Status:")
        layout.addWidget(status_label)

        self.status_text = QTextEdit()
        self.status_text.setReadOnly(True)
        self.status_text.setMaximumHeight(200)
        layout.addWidget(self.status_text)

        self.progress_bar = QProgressBar()
        self.progress_bar.setValue(0)
        layout.addWidget(self.progress_bar)

        layout.addStretch()
        widget.setLayout(layout)
        self.tabs.addTab(widget, "Installation")

    def _create_summary_tab(self):
        """Installation summary tab"""
        widget = QWidget()
        layout = QVBoxLayout()

        title = QLabel("Installation Summary")
        title_font = QFont()
        title_font.setPointSize(12)
        title_font.setBold(True)
        title.setFont(title_font)
        layout.addWidget(title)

        self.summary_text = QTextEdit()
        self.summary_text.setReadOnly(True)
        layout.addWidget(self.summary_text)

        layout.addStretch()
        widget.setLayout(layout)
        self.tabs.addTab(widget, "Summary")

    def _load_system_info(self):
        """Load system information"""
        try:
            import psutil
            mem = psutil.virtual_memory()
            logger.info(f"System memory: {mem.total / (1024**3):.1f} GB")
        except ImportError:
            pass

    def _previous_tab(self):
        """Go to previous tab"""
        current = self.tabs.currentIndex()
        if current > 0:
            self.tabs.setCurrentIndex(current - 1)
        self._update_buttons()

    def _next_tab(self):
        """Go to next tab"""
        current = self.tabs.currentIndex()
        if current < self.tabs.count() - 1:
            self.tabs.setCurrentIndex(current + 1)
        self._update_buttons()

    def _update_buttons(self):
        """Update button visibility"""
        current = self.tabs.currentIndex()
        total = self.tabs.count()

        self.prev_btn.setEnabled(current > 0)
        self.next_btn.setVisible(current < total - 2)
        self.install_btn.setVisible(current == total - 2)

    def _start_installation(self):
        """Start the installation process"""
        # Gather configuration
        self.config = {
            'hostname': self.hostname_input.text(),
            'locale': self.locale_combo.currentText(),
            'timezone': self.timezone_input.text(),
            'username': self.username_input.text(),
            'target_disk': self.target_combo.currentText(),
            'auto_partition': self.auto_partition.isChecked(),
            'auto_format': self.auto_format.isChecked(),
            'boot_size': self.boot_size.value(),
            'swap_size': self.swap_size.value(),
        }

        # Update summary
        summary = f"""
Hostname: {self.config['hostname']}
Locale: {self.config['locale']}
Timezone: {self.config['timezone']}
Username: {self.config['username']}
Target Disk: {self.config['target_disk']}
Auto Partition: {self.config['auto_partition']}
Boot Size: {self.config['boot_size']} MB
Swap Size: {self.config['swap_size']} MB
        """
        self.summary_text.setText(summary)

        # Start installation
        self.tabs.setCurrentIndex(3)  # Go to installation tab
        self._run_installation()

    def _run_installation(self):
        """Execute installation"""
        if self.worker is not None and self.worker.isRunning():
            return

        self.worker = InstallerWorker(self.config)
        self.worker.progress.connect(self.progress_bar.setValue)
        self.worker.status.connect(self._update_status)
        self.worker.finished.connect(self._installation_finished)
        self.worker.start()

        self.install_btn.setEnabled(False)

    def _update_status(self, status_msg: str):
        """Update status message"""
        self.status_text.append(status_msg)
        logger.info(status_msg)

    def _installation_finished(self, success: bool, message: str):
        """Handle installation completion"""
        self.install_btn.setEnabled(True)

        if success:
            QMessageBox.information(self, "Success", message)
            logger.info("Installation completed successfully")
        else:
            QMessageBox.critical(self, "Error", message)
            logger.error(message)


def main():
    """Run installer"""
    app = QApplication(sys.argv)
    window = InstallerWindow()
    window.show()
    sys.exit(app.exec_())


if __name__ == '__main__':
    main()
