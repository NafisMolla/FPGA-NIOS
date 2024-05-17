# Wav Player on Nios II fpga

![image](https://github.com/NafisMolla/FPGA-NIOS/assets/37641864/df007e72-62c6-40f9-91c2-964c7b02feff)

![image](https://github.com/NafisMolla/FPGA-NIOS/assets/37641864/7baeca47-b9ca-408d-817d-ed4ab3876818)

This project implements an audio player on the Nios II FPGA board. The player uses various hardware and software components to read and play audio files from a FAT file system. The program initializes the UART for communication and sets up the disk I/O interface using the FAT file system. A timer is used to manage periodic tasks, such as disk I/O operations.

The project includes a simple user interface that allows users to play, pause, stop, and skip through audio files using hardware buttons. The state of the player, including the current play mode and selected file, is displayed on an LCD screen. The audio files are stored on an SD card, and the program reads these files into memory for playback.

Audio playback is handled by the Altera audio IP core, which reads data from the buffers and plays it through the FPGA's audio output. The program supports different playback modes, including normal, double-speed, half-speed, and mono. The appropriate mode is selected based on the position of the hardware switches.

The main loop of the program handles user input, manages the playback state, and updates the display. When a file is being played, the program reads chunks of audio data from the file and writes them to the audio buffers, ensuring smooth playback. Various interrupt service routines handle button presses and timer interrupts, allowing the program to respond to user actions and manage timing accurately.

Overall, this project demonstrates the integration of multiple hardware components and software modules to create a functional audio player on an FPGA platform, providing a simple and effective way to play and control audio files.
