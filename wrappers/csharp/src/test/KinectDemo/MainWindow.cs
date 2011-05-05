using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;
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
		private PreviewWindow videoPreviewWindow = new PreviewWindow();
		
		/// <summary>
		/// Preview window for depth
		/// </summary>
		private PreviewWindow depthPreviewWindow = new PreviewWindow();
		
		/// <summary>
		/// Constructor
		/// </summary>
		public MainWindow()
		{
			// Initialize UI stuff
			this.InitializeComponents();
			
			// Add some event handlers for the preview windows
			this.videoPreviewWindow.FormClosing += HandleVideoPreviewWindowFormClosing;
			this.depthPreviewWindow.FormClosing += HandleDepthPreviewWindowFormClosing;
			
			// Update device list
			this.UpdateDeviceList();
			
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
				this.connectButton.Enabled = true;
				this.disconnectButton.Enabled = false;
			}
			else
			{
				// Disable buttons
				this.connectButton.Enabled = false;
				this.disconnectButton.Enabled = false;
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
			if(this.kinect != null)
			{
				this.Disconnect();
			}
			
			// Create instance
			this.kinect = new Kinect(deviceID);
			
			// Open kinect
			this.kinect.Open();
			
			// Setup image handlers
			this.kinect.VideoCamera.DataReceived += HandleKinectVideoCameraDataReceived;
			this.kinect.DepthCamera.DataReceived += HandleKinectDepthCameraDataReceived;
			
			// Update video/depth modes
			this.UpdateModeList();
			
			// Enable video/depth mode chooser
			this.selectVideoModeGroup.Enabled = true;
			
			// Disable connect button and enable the disconnect one
			this.disconnectButton.Enabled = true;
			this.connectButton.Enabled = false;
		}
		
		/// <summary>
		/// Disconnects from teh currently connected Kinect device
		/// </summary>
		private void Disconnect()
		{
			// Disconnect from the kinect
			this.kinect.Close();
			this.kinect = null;
			
			// Hide preview windows
			this.videoPreviewWindow.Visible = false;
			this.depthPreviewWindow.Visible = false;
			
			// Disable video/depth mode chooser
			this.selectVideoModeGroup.Enabled = false;
			
			// Disable connect button and enable the disconnect one
			this.disconnectButton.Enabled = false;
			this.connectButton.Enabled = true;
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
			Console.Write(".");
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
			int index = this.selectDepthModeCombo.SelectedIndex;
			
			Console.WriteLine("Selected Depth Moe: " + index);
			
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
				this.kinect.DepthCamera.Stop();
				this.kinect.DepthCamera.Mode = this.kinect.DepthCamera.Modes[index];
				this.kinect.DepthCamera.Start();
				this.depthPreviewWindow.Visible = true;
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