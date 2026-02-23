import sys
import re
import subprocess
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QLabel, QPushButton, QFileDialog, QCheckBox, QGridLayout, QSlider, QHBoxLayout, QMessageBox
from PyQt5.QtCore import Qt  # Import the Qt namespace for orientation

class TraceCutterApp(QWidget):
    def __init__(self):
        super().__init__()

        self.filename = None
        self.num_cluster_x = 0
        self.num_cluster_y = 0
        self.trace_start_time = 0
        self.trace_end_time = 0

        self.initUI()

    def initUI(self):
        # Main layout
        self.layout = QVBoxLayout()

        # Welcome Label
        self.welcome_label = QLabel("Welcome to SoftHier Trace Cutter", self)
        self.layout.addWidget(self.welcome_label)

        # Select File Button
        self.select_button = QPushButton("Please select the original trace", self)
        self.select_button.clicked.connect(self.select_file)
        self.layout.addWidget(self.select_button)

        self.setLayout(self.layout)
        self.setWindowTitle('SoftHier Trace Cutter')
        self.setGeometry(300, 300, 400, 200)

    def select_file(self):
        # Select a file
        self.filename, _ = QFileDialog.getOpenFileName(self, 'Select Trace File', '', 'Text Files (*.txt);;All Files (*)')

        if not self.filename:
            return

        try:
            self.analyze_trace(self.filename)
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to analyze the file: {str(e)}")
            return

        # Show cluster selection and slider after file is loaded successfully
        self.show_cluster_selection()
        self.show_time_slider()
        self.show_generate_button()

    def analyze_trace(self, filename):

        self.num_cluster_x = None
        self.num_cluster_y = None
        self.trace_start_line = None
        self.trace_end_line = None

        command = 'grep "kernel/sc_object" ' + filename + ' -n | cut -d":" -f1'
        self.trace_start_line = int(subprocess.run(command, shell=True, capture_output=True, text=True).stdout) + 1

        command = 'grep "Simulation stopped by user" ' + filename + ' -n | cut -d":" -f1'
        self.trace_end_line = int(subprocess.run(command, shell=True, capture_output=True, text=True).stdout) - 2

        command = f"sed -n \'{self.trace_start_line}p\' " + filename
        pattern = r':\s(\d+):'
        text = subprocess.run(command, shell=True, capture_output=True, text=True).stdout
        match = re.search(pattern, text)
        if match:
            self.trace_start_time = int(match.group(1))
        else:
            raise ValueError("trace_start_time not found in the trace file.")

        command = f"sed -n \'{self.trace_end_line}p\' " + filename
        pattern = r':\s(\d+):'
        text = subprocess.run(command, shell=True, capture_output=True, text=True).stdout
        match = re.search(pattern, text)
        if match:
            self.trace_end_time = int(match.group(1))
        else:
            raise ValueError("trace_end_time not found in the trace file.")

        command = 'grep "SystemInfo" ' + filename
        pattern = r'num_cluster_x\s*=\s*(\d+),\s*num_cluster_y\s*=\s*(\d+)'
        text = subprocess.run(command, shell=True, capture_output=True, text=True).stdout
        match = re.search(pattern, text)
        if match:
            # Extract the values using groups
            self.num_cluster_x = int(match.group(1))
            self.num_cluster_y = int(match.group(2))
        else:
            raise ValueError("num_cluster_x and num_cluster_y not found in the trace file.")

    def show_cluster_selection(self):
        # Cluster selection grid
        self.grid_layout = QGridLayout()
        self.grid_layout.addWidget(QLabel("Please select which cluster's trace you want to look:"), 0, 0, 1, self.num_cluster_y)

        self.cluster_checkboxes = []
        for x in range(self.num_cluster_x):
            row = []
            for y in range(self.num_cluster_y):
                checkbox = QCheckBox(f'({x}, {y})')
                self.grid_layout.addWidget(checkbox, x+1, y)
                row.append(checkbox)
            self.cluster_checkboxes.append(row)

        self.layout.addLayout(self.grid_layout)

    def show_time_slider(self):
        # Time slider for selecting runtime length
        self.time_slider_layout = QVBoxLayout()

        # Create a label that will display the start, end, and current value
        self.time_slider_label = QLabel(f"Select trace runtime length: {self.trace_start_time} ns to {self.trace_end_time} ns", self)
        self.current_value_label = QLabel(f"Selected value: {self.trace_start_time} ns", self)  # Initial selected value label

        # Create the time slider
        self.time_slider = QSlider(Qt.Horizontal)
        self.time_slider.setMinimum(self.trace_start_time)
        self.time_slider.setMaximum(self.trace_end_time)
        self.time_slider.setValue(self.trace_start_time)  # Set initial value to trace start time
        self.time_slider.setSingleStep(1)  # You can adjust this if needed

        # Connect the slider's value change to a function that updates the label
        self.time_slider.valueChanged.connect(self.update_slider_value)

        # Add the labels and the slider to the layout
        self.time_slider_layout.addWidget(self.time_slider_label)
        self.time_slider_layout.addWidget(self.time_slider)
        self.time_slider_layout.addWidget(self.current_value_label)

        self.layout.addLayout(self.time_slider_layout)

    def update_slider_value(self):
        # This function will be called whenever the slider value changes
        current_value = self.time_slider.value()
        self.current_value_label.setText(f"Selected value: {current_value} ns")


    def show_generate_button(self):
        # Generate button
        self.gen_button = QPushButton("Generate Reduced Trace", self)
        self.gen_button.clicked.connect(self.generate_trace)
        self.layout.addWidget(self.gen_button)

    def generate_trace(self):
        # Collect selected clusters
        selected_clusters = []
        for x in range(self.num_cluster_x):
            for y in range(self.num_cluster_y):
                if self.cluster_checkboxes[x][y].isChecked():
                    selected_clusters.append((x, y))

        end_point = self.time_slider.value()
        self.gen_trace(end_point, selected_clusters)

    def gen_trace(self, end_point, selected_cluster_id_list):
        # Placeholder for your custom trace generation logic
        print(f"End Point: {end_point}")
        print(f"Selected Clusters: {selected_cluster_id_list}")


if __name__ == "__main__":
    app = QApplication(sys.argv)
    ex = TraceCutterApp()
    ex.show()
    sys.exit(app.exec_())
