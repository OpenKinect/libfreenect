using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;
using System.Threading;
using freenect;

namespace KinectDemo
{
	public partial class MainWindow : Form
	{
		/// <summary>
		/// Current kinect device
		/// </summary>
		private Kinect kinect = null;
		
		/// <summary>
		/// Preview window for video
		/// </summary>
		private PreviewWindow videoPreviewWindow = new VideoPreviewWindow();
		
		/// <summary>
		/// Preview window for depth
		/// </summary>
		private PreviewWindow depthPreviewWindow = new DepthPreviewWindow();
		
		/// <summary>
		/// Update timer for tilt/motor status
		/// </summary>
		private System.Windows.Forms.Timer statusUpdateTimer = new System.Windows.Forms.Timer();
		
		/// <summary>
		/// Is a device connected and running?
		/// </summary>
		private volatile bool isRunning = false;
		
		/// <summary>
		/// Constructor
		/// </summary>
		public MainWindow()
		{
			// Initialize UI stuff
			this.InitializeComponents();
			
			// Initialize update timer
			this.statusUpdateTimer.Interval = 500;
			this.statusUpdateTimer.Tick += this.HandleStatusUpdateTimerTick;
			
			// Add some event handlers for the preview windows
			this.videoPreviewWindow.FormClosing += HandleVideoPreviewWindowFormClosing;
			this.depthPreviewWindow.FormClosing += HandleDepthPreviewWindowFormClosing;
			
			// Show preview windows for the first time so controls onboard can load up
			this.videoPreviewWindow.Visible = this.depthPreviewWindow.Visible = true;
			
			// Update device list
			this.UpdateDeviceList();
			
			// Hide preview windows again, controls are done loading by now
			this.videoPreviewWindow.Visible = this.depthPreviewWindow.Visible = true;
			
		}
		
		/// <summary>
		/// Updates the list of devices shown in the GUI
		/// </summary>
		private void UpdateDeviceList()
		{
			// Clear old
			this.selectDeviceCombo.Items.Clear();
			
			// Get count
			int deviceCount = Kinect.DeviceCount;
			
			// Fill in combo box
			for(int i = 0; i < deviceCount; i++)	
			{
				this.selectDeviceCombo.Items.Add("Device " + i);
			}
			
			// Do we have anything to auto-select?
			if(deviceCount > 0)	
			{
				this.selectDeviceCombo.SelectedIndex = 0;
				
				// Enable buttons
				this.connectButton.Visible = this.connectButton.Enabled = true;
				this.refreshButton.Visible = this.refreshButton.Enabled = true;
				this.disconnectButton.Visible = false;
			}
			else
			{
				// Disable buttons
				this.connectButton.Visible = false;
				this.refreshButton.Visible = false;
				this.disconnectButton.Visible = false;
			}
		}
		
		/// <summary>
		/// Update list of modes
		/// </summary>
		private void UpdateModeList()
		{
			// Go through video modes and add em
			this.selectVideoModeCombo.Items.Clear();
			this.selectVideoModeCombo.Items.Add("Disabled");
			foreach(var mode in this.kinect.VideoCamera.Modes)
			{
				string videoMode = mode.Width + "x" + mode.Height + " : " + mode.Format;
				this.selectVideoModeCombo.Items.Add(videoMode);
			}
			
			// Autoselect first mode
			if(this.kinect.VideoCamera.Modes.Length > 0)
			{
				this.selectVideoModeCombo.SelectedIndex = 0;
			}
			
			// Go through depth modes and add em
			this.selectDepthModeCombo.Items.Clear();
			this.selectDepthModeCombo.Items.Add("Disabled");
			foreach(var mode in this.kinect.DepthCamera.Modes)
			{
				string depthMode = mode.Width + "x" + mode.Height + " : " + mode.Format;
				this.selectDepthModeCombo.Items.Add(depthMode);
			}
			
			// Autoselect first mode
			if(this.kinect.DepthCamera.Modes.Length > 0)
			{
				this.selectDepthModeCombo.SelectedIndex = 0;
			}
		}
		
		/// <summary>
		/// Connects to the specified device
		/// </summary>
		private void Connect(int deviceID)
		{
			// If a device is already connected, disconnect
			if(this.isRunning)
			{
				this.Disconnect();
			}
			
			// Create instance
			this.kinect = new Kinect(deviceID);
			
			// Open kinect
			this.kinect.Open();
			
			// Set tilt to 0 to start with
			this.motorTiltUpDown.Value = 0;
			
			// Setup image handlers
			this.kinect.VideoCamera.DataReceived += HandleKinectVideoCameraDataReceived;
			this.kinect.DepthCamera.DataReceived += HandleKinectDepthCameraDataReceived;
			
			// LED is set to none to start
			this.kinect.LED.Color = LEDColor.None;
			this.selectLEDColorCombo.SelectedIndex = 0;
			
			// Update video/depth modes
			this.UpdateModeList();
			
			// Enable video/depth mode chooser
			this.selectVideoModeGroup.Enabled = true;
			
			// Start updating status
			this.statusUpdateTimer.Enabled = true;
			
			// Disable connect button and enable the disconnect one
			this.disconnectButton.Visible = true;
			this.connectButton.Visible = false;
			this.refreshButton.Visible = false;
			this.selectDeviceCombo.Enabled = false;
			
			// Enable content areas
			this.contentPanelLeft.Enabled = this.contentPanelRight.Enabled = true;
			
			// Now running
			this.isRunning = true;
		}
		
		/// <summary>
		/// Disconnects from teh currently connected Kinect device
		/// </summary>
		private void Disconnect()
		{
			// Are we running?
			if(this.isRunning == false)
			{
				return;	
			}
			
			// Stop updating status
			this.statusUpdateTimer.Enabled = false;
			
			// No longer running
			this.isRunning = false;
			
			// Disconnect from the kinect
			this.kinect.Close();
			this.kinect = null;
			
			// Hide preview windows
			this.videoPreviewWindow.Visible = false;
			this.depthPreviewWindow.Visible = false;
			
			// Disable video/depth mode chooser
			this.selectVideoModeGroup.Enabled = false;
			
			// Disable disconnect button and enable the connect/refresh ones
			this.disconnectButton.Visible = false;
			this.connectButton.Visible = true;
			this.refreshButton.Visible = true;
			this.selectDeviceCombo.Enabled = true;
			
			// Enable content areas
			this.contentPanelLeft.Enabled = this.contentPanelRight.Enabled = false;
		}
		
		/// <summary>
		/// 
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="DepthCamera.DataReceivedEventArgs"/>
		/// </param>
		private void HandleKinectDepthCameraDataReceived (object sender, DepthCamera.DataReceivedEventArgs e)
		{
			// Let the preview window handle new back buffer data
			this.depthPreviewWindow.HandleBackBufferUpdate();
			
			// Set the depth camera's buffer to the new back buffer for the preview window
			this.kinect.DepthCamera.DataBuffer = this.depthPreviewWindow.BackBuffer;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="VideoCamera.DataReceivedEventArgs"/>
		/// </param>
		private void HandleKinectVideoCameraDataReceived (object sender, VideoCamera.DataReceivedEventArgs e)
		{
			// Let the preview window handle new back buffer data
			this.videoPreviewWindow.HandleBackBufferUpdate();
			
			// Set the video camera's buffer to the new back buffer for the preview window
			this.kinect.VideoCamera.DataBuffer = this.videoPreviewWindow.BackBuffer;
		}
		
		/// <summary>
		/// Update status panes
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="EventArgs"/>
		/// </param>
		private void HandleStatusUpdateTimerTick (object sender, EventArgs e)
		{
			this.kinect.UpdateStatus();
			this.motorCurrentTiltLabel.Text = "Current Tilt: " + this.kinect.Motor.Tilt;
			this.motorTiltStatusLabel.Text = "Tilt Status: " + this.kinect.Motor.TiltStatus.ToString();
			this.accelerometerXValueLabel.Text = Math.Round(this.kinect.Accelerometer.X, 2).ToString();
			this.accelerometerYValueLabel.Text = Math.Round(this.kinect.Accelerometer.Y, 2).ToString();
			this.accelerometerZValueLabel.Text = Math.Round(this.kinect.Accelerometer.Z, 2).ToString();
		}
		
		/// <summary>
		/// Selected different LED color
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="EventArgs"/>
		/// </param>
		private void HandleSelectLEDColorComboSelectedIndexChanged (object sender, EventArgs e)
		{
			this.kinect.LED.Color = (LEDColor)Enum.Parse(typeof(LEDColor), this.selectLEDColorCombo.SelectedItem.ToString());
		}
		
		/// <summary>
		/// Motor tilt changed
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="EventArgs"/>
		/// </param>
		private void HandleMotorTiltUpDownValueChanged (object sender, EventArgs e)
		{
			this.kinect.Motor.Tilt = (double)this.motorTiltUpDown.Value;
		}
		
		/// <summary>
		/// Handle form being closed. Here we make sure we are closed down
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="FormClosingEventArgs"/>
		/// </param>
		private void HandleFormClosing (object sender, FormClosingEventArgs e)
		{
			this.Disconnect();
		}
		
		/// <summary>
		/// Handle refresh button
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="EventArgs"/>
		/// </param>
		private void HandleRefreshButtonClick (object sender, EventArgs e)
		{
			this.UpdateDeviceList();
		}
		
		/// <summary>
		/// Handle connnect button
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="EventArgs"/>
		/// </param>
		private void HandleConnectButtonClick (object sender, EventArgs e)
		{
			this.Connect(this.selectDeviceCombo.SelectedIndex);
		}
		
		/// <summary>
		/// Handle disconnect button
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="EventArgs"/>
		/// </param>
		private void HandleDisconnectButtonClick (object sender, EventArgs e)
		{
			this.Disconnect();
		}
		
		/// <summary>
		/// Handle depth mode being changed
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="EventArgs"/>
		/// </param>
		private void HandleSelectDepthModeComboSelectedIndexChanged (object sender, EventArgs e)
		{
			// Check to see if we are actually connected
			if(this.isRunning == false)
			{
				// Not running, shouldn't even be here
				return;
			}
			
			// Get index selected
			int index = this.selectDepthModeCombo.SelectedIndex;
			
			// 0 means "Disabled", otherwise, it's a depth format
			if(index == 0)
			{
				// Disabled
				this.depthPreviewWindow.Visible = false;
				this.kinect.DepthCamera.Stop();
			}
			else if(index > 0)
			{
				// Index is really 1 less (because "Disabled" we added is first)
				index -= 1;
				
				// Set mode
				DepthFrameMode mode = this.kinect.DepthCamera.Modes[index];
				this.kinect.DepthCamera.Stop();
				this.kinect.DepthCamera.Mode = mode;
				
				// Wait until preview window is not busy
				while(this.depthPreviewWindow.IsBusy);
				
				// Resize preview window
				this.depthPreviewWindow.Resize(mode);
				
				// Set new back buffer for depth camera
				this.kinect.DepthCamera.DataBuffer = this.depthPreviewWindow.BackBuffer;
				
				// Start up camera again				
				this.kinect.DepthCamera.Start();
				this.depthPreviewWindow.Visible = true;
			}
		}
		
		/// <summary>
		/// Handle video mode being changed
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="EventArgs"/>
		/// </param>
		private void HandleSelectVideoModeComboSelectedIndexChanged (object sender, EventArgs e)
		{
			// Check to see if we are actually connected
			if(this.isRunning == false)
			{
				// Not running, shouldn't even be here
				return;
			}
			
			// Get index selected
			int index = this.selectVideoModeCombo.SelectedIndex;
			
			// 0 means "Disabled", otherwise, it's a depth format
			if(index == 0)
			{
				// Disabled
				this.videoPreviewWindow.Visible = false;
				this.kinect.VideoCamera.Stop();
			}
			else if(index > 0)
			{
				// Index is really 1 less (because "Disabled" we added is first)
				index -= 1;
				
				// Set mode
				VideoFrameMode mode = this.kinect.VideoCamera.Modes[index];
				this.kinect.VideoCamera.Stop();
				this.kinect.VideoCamera.Mode = mode;
				
				// Wait until preview window is not busy
				while(this.videoPreviewWindow.IsBusy);
				
				// Resize preview window
				this.videoPreviewWindow.Resize(mode);
				
				// Set new back buffer for video camera
				this.kinect.VideoCamera.DataBuffer = this.videoPreviewWindow.BackBuffer;
				
				// Start up camera again
				this.kinect.VideoCamera.Start();
				this.videoPreviewWindow.Visible = true;
			}
		}
		
		/// <summary>
		/// 
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="FormClosingEventArgs"/>
		/// </param>
		private void HandleDepthPreviewWindowFormClosing (object sender, FormClosingEventArgs e)
		{
			e.Cancel = true;
			this.selectDepthModeCombo.SelectedIndex = 0;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="FormClosingEventArgs"/>
		/// </param>
		private void HandleVideoPreviewWindowFormClosing (object sender, FormClosingEventArgs e)
		{
			e.Cancel = true;
			this.selectVideoModeCombo.SelectedIndex = 0;
		}
	}
}