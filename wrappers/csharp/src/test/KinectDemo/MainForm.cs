/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */
using System;
using System.IO;
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.Threading;
using LibFreenect;
using OpenTK;
using OpenTK.Graphics.OpenGL;

namespace KinectDemo
{
	public class MainForm : Form
	{
		// Kinect Object
		private Kinect kinect;
		
		// W/e this gamma shit is from the glview example
		private UInt16[] gamma = new UInt16[2048];	
		
		// Just a simple optimization. Dont need to do this multiply every time;
		private const int imgSize = 640 * 480;
		
		// Thread for updating status continuously
		private Thread updateStatusThread;
		
		// Stuff for FPS counters
		private int depthFrameCount = 0;
		private int videoFrameCount = 0;
		private DateTime depthLastFrame = DateTime.Now;
		private DateTime videoLastFrame = DateTime.Now;
		private int depthFPS = 0;
		private int videoFPS = 0;
		
		// Stuff for GL
		uint depthTexture;
		uint rgbTexture;
		
		byte[] rgbBufferBack;
		byte[] rgbBufferMid;
		byte[] rgbBufferFront;
		GCHandle rgbHandleBack;
		GCHandle rgbHandleMid;
		GCHandle rgbHandleFront;
		
		byte[] depthBufferBack;
		byte[] depthBufferMid;
		byte[] depthBufferFront;
		GCHandle depthHandleBack;
		GCHandle depthHandleMid;
		GCHandle depthHandleFront;

		
		/// <summary>
		/// Constructor
		/// </summary>
		public MainForm() : base()
		{
			// Initialize GUI Elements
			this.InitializeComponents();
			
			for (int i = 0 ; i < 2048; i++) 
			{
				double v = i / 2048.0;
				v = Math.Pow((double)v, 3.0) * 6.0;
				gamma[i] = (UInt16)(v * 6.0 * 256.0);
			}
			
			// Allocate swap bufers
			this.rgbBufferFront = new byte[VideoCamera.DataFormatSizes[VideoCamera.DataFormatOption.RGB]];
			this.rgbBufferMid 	= new byte[VideoCamera.DataFormatSizes[VideoCamera.DataFormatOption.RGB]];
			this.rgbBufferBack 	= new byte[VideoCamera.DataFormatSizes[VideoCamera.DataFormatOption.RGB]];
			this.rgbHandleFront = GCHandle.Alloc(this.rgbBufferFront, GCHandleType.Pinned);
			this.rgbHandleMid 	= GCHandle.Alloc(this.rgbBufferMid, GCHandleType.Pinned);
			this.rgbHandleBack 	= GCHandle.Alloc(this.rgbBufferBack, GCHandleType.Pinned);
			
			this.depthBufferFront 	= new byte[640 * 480 * 3];
			this.depthBufferMid 	= new byte[640 * 480 * 3];
			this.depthBufferBack 	= new byte[640 * 480 * 3];
			this.depthHandleFront 	= GCHandle.Alloc(this.depthBufferFront, GCHandleType.Pinned);
			this.depthHandleMid 	= GCHandle.Alloc(this.depthBufferMid, GCHandleType.Pinned);
			this.depthHandleBack 	= GCHandle.Alloc(this.depthBufferBack, GCHandleType.Pinned);
			
			// Check for kinect devices
			if(Kinect.DeviceCount == 0)
			{
				MessageBox.Show("There are no Kinect devices connected to your system. Please reopen the program after you have atleast 1 Kinect connected.");
			}
			else
			{
				Kinect.LogLevel = Kinect.LogLevelOptions.Warning;
				Kinect.Log += new LogEventHandler(HandleLogMessage);
				
				// Atleast one connected, fill the select box with w/e devices available
				for(int i = 0; i < Kinect.DeviceCount; i++)
				{
					this.kinectDeviceSelectCombo.Items.Add("Device " + i);
				}
				this.kinectDeviceSelectCombo.SelectedIndex = 0;
				
				// Enable the toolbar so we can do things...
				this.mainToolbar.Enabled = true;
			}
		}
		
		private void Connect()
		{
			// Create device instance and open the connection
			this.kinect = new Kinect(this.kinectDeviceSelectCombo.SelectedIndex);
			this.kinect.Open();
			
			// Set the buffer to back first
			this.kinect.VideoCamera.DataBuffer = rgbHandleBack.AddrOfPinnedObject();
			this.kinect.DepthCamera.DataBuffer = depthHandleBack.AddrOfPinnedObject();
			
			// Set modes
			this.kinect.VideoCamera.DataFormat = VideoCamera.DataFormatOption.RGB;
			this.kinect.DepthCamera.DataFormat = DepthCamera.DataFormatOption.Format11Bit;
			
			// Enable controls and info
			this.bottomPanel.Enabled = true;
			
			// Set LED to none
			this.ledControlCombo.SelectedIndex = 0;
			
			// Set Motor to 0
			this.motorControlTrack.Value = 0;
			
			// Start video camera
			this.kinect.VideoCamera.DataReceived += new VideoCamera.DataReceivedEventHandler(this.HandleVideoDataReceived);
			this.kinect.VideoCamera.Start();
			
			// Start depth camera
			this.kinect.DepthCamera.DataReceived += new DepthCamera.DataReceivedEventHandler(this.HandleDepthDataReceived);
			this.kinect.DepthCamera.Start();
			
			// Enable disconnect
			this.connectButton.Enabled = false;
			this.disconnectButton.Enabled = true;
			
			// Enable update timer
			this.infoUpdateTimer.Enabled = true;
			
			// Start update status thread
			this.updateStatusThread = new Thread(delegate()
			{
				while(true)
				{
					if(this.kinect == null)
					{
						continue;
					}
					
					// Update the status for this device
					this.kinect.UpdateStatus();
					
					// Let the kinect library handle any pending stuff on the usb streams.
					Kinect.ProcessEvents();
					
					try
					{
						this.imagePanel.Invalidate();
					}
					catch(Exception e)
					{
					}
				}
			});
			this.updateStatusThread.Start();
		}
		
		private void Disconnect()
		{
			// Stop the update thread
			this.updateStatusThread.Abort();
			while(this.updateStatusThread.IsAlive)
			{
				// wait...
				Application.DoEvents();
				Thread.Sleep(1000);
				Console.WriteLine("waiting for disconnect...");
			}
			
			this.updateStatusThread = null;
			
			// Disable update timer first
			this.infoUpdateTimer.Enabled = false;
			
			// Disconnect from the device
			this.kinect.Close();
			this.kinect = null;
			
			// Disable controls again
			this.bottomPanel.Enabled = false;
			
			// Enable connect
			this.connectButton.Enabled = true;
			this.disconnectButton.Enabled = false;

		}
		
		/// <summary>
		/// Handles a log message coming in from the low level library
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="LogEventArgs"/>
		/// </param>
		private void HandleLogMessage(object sender, LogEventArgs e)
		{
			
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
		private void HandleVideoDataReceived(object sender, VideoCamera.DataReceivedEventArgs e)
		{
			if(this.kinect == null || this.kinect.IsOpen == false)
			{
				return;
			}
			if((DateTime.Now - this.videoLastFrame).TotalMilliseconds >= 1000)
			{
				this.videoFPS = this.videoFrameCount;
				this.videoFrameCount = 0;
				this.videoLastFrame = DateTime.Now;
			}
			else
			{
				this.videoFrameCount++;
			}
			
			// Swap mid and back
			GCHandle tmp = rgbHandleBack;
			rgbHandleMid = rgbHandleBack;
			rgbHandleBack = tmp;
			
			// Set kinect video camera's buffer to new back buffer
			this.kinect.VideoCamera.DataBuffer = rgbHandleBack.AddrOfPinnedObject();
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
		private void HandleDepthDataReceived(object sender, DepthCamera.DataReceivedEventArgs e)
		{
			if(this.kinect == null || this.kinect.IsOpen == false)
			{
				return;
			}
			if((DateTime.Now - this.depthLastFrame).TotalMilliseconds >= 1000)
			{
				this.depthFPS = this.depthFrameCount;
				this.depthFrameCount = 0;
				this.depthLastFrame = DateTime.Now;
			}
			else
			{
				this.depthFrameCount++;
			}
			
			try
			{
				// Swap mid and back
				unsafe
				{
					byte *ptrMid 	= (byte *)this.depthHandleMid.AddrOfPinnedObject();
					Int16 *ptrBack 	= (Int16 *)this.depthHandleBack.AddrOfPinnedObject();
					int dim 		= 640 * 480;
					int i 			= 0;
					for (i = 0; i < dim; i++)
					{
						Int16 pval 	= (Int16)this.gamma[ptrBack[i]];
						Int16 lb 	= (Int16)(pval & 0xff);
						switch (pval>>8)
						{
							case 0:
								*ptrMid++ = 255;
								*ptrMid++ = (byte)(255 - lb);
								*ptrMid++ = (byte)(255 - lb);
								break;
							case 1:
								*ptrMid++ = 255;
								*ptrMid++ = (byte)lb;
								*ptrMid++ = 0;
								break;
							case 2:
								*ptrMid++ = (byte)(255 - lb);
								*ptrMid++ = 255;
								*ptrMid++ = 0;
								break;
							case 3:
								*ptrMid++ = 0;
								*ptrMid++ = 255;
								*ptrMid++ = (byte)lb;
								break;
							case 4:
								*ptrMid++ = 0;
								*ptrMid++ = (byte)(255 - lb);
								*ptrMid++ = 255;
								break;
							case 5:
								*ptrMid++ = 0;
								*ptrMid++ = 0;
								*ptrMid++ = (byte)(255 - lb);
								break;
							default:
								*ptrMid++ = 0;
								*ptrMid++ = 0;
								*ptrMid++ = 0;
								break;
						}
					}
				}
			}
			catch(Exception ex)
			{
				Console.WriteLine(ex.ToString());
			}
		}
		
		/// <summary>
		/// Sets the LED on w/e Kinect device is open right now
		/// </summary>
		/// <param name="colorName">
		/// String version of any of the values in KinectLED.ColorOption. Example "Green".
		/// </param>
		private void SetLED(string colorName)
		{
			if(this.kinect == null)
			{
				return;	
			}
			this.kinect.LED.Color = (LED.ColorOption)Enum.Parse(typeof(LED.ColorOption), colorName);
		}
		
		/// <summary>
		/// Sets the tilt angle on w/e Kinect device is open right now
		/// </summary>
		/// <param name="value">
		/// Tilt angle between -1.0 and 1.0
		/// </param>
		private void SetMotorTilt(int value)
		{
			if(this.kinect == null)
			{
				return;	
			}
			this.kinect.Motor.Tilt = (float)value / 100.0f;
		}
		
		/// <summary>
		/// Update info panel
		/// </summary>
		private void UpdateInfoPanel()
		{
			if(this.kinect == null)
			{
				return;
			}
			
			this.Text = "FPS(RGB):" + this.videoFPS + "   FPS(DEPTH):" + this.depthFPS;
			
			this.kinect.UpdateStatus();
			this.motorTiltStatusLabel.Text = "Tilt Status: " + this.kinect.Motor.TiltStatus;
			this.motorTiltAngleLabel.Text = "Tilt Angle: " + this.kinect.Motor.Tilt;
			this.accelXLabel.Text = "Accel X: " + Math.Round(this.kinect.Accelerometer.X, 3);
			this.accelYLabel.Text = "Accel Y: " + Math.Round(this.kinect.Accelerometer.Y, 3);
			this.accelZLabel.Text = "Accel Z: " + Math.Round(this.kinect.Accelerometer.Z, 3);
		}
		
		/// <summary>
		/// Load time config of the GL window
		/// </summary>
		private void ImagePanel_Load()
		{
			GL.ClearColor(Color.Blue);
			GL.ClearDepth(1.0);
			GL.DepthFunc(DepthFunction.Less);
			GL.Disable(EnableCap.DepthTest);
			GL.Enable(EnableCap.Blend);
			GL.BlendFunc(BlendingFactorSrc.SrcAlpha, BlendingFactorDest.OneMinusSrcAlpha);
			GL.ShadeModel(ShadingModel.Smooth);
			
			GL.GenTextures(1, out this.rgbTexture);
			GL.BindTexture(TextureTarget.Texture2D, this.rgbTexture);
			GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Linear);
			GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter, (int)TextureMinFilter.Linear);
			GL.GenTextures(1, out this.depthTexture);
			GL.BindTexture(TextureTarget.Texture2D, this.depthTexture);
			GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Linear);
			GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter, (int)TextureMinFilter.Linear);
			this.ResizeScene();
		}
		
		private void ResizeScene()
		{
			GL.Viewport(0, 0, 1280, 480);
			GL.MatrixMode(MatrixMode.Projection);
			GL.LoadIdentity();
			GL.Ortho(0, 1280, 480, 0, -1.0f, 1.0f);
			GL.MatrixMode(MatrixMode.Modelview);
		}
		
		/// <summary>
		/// Painting stuff in the GL window
		/// </summary>
		private void ImagePanel_Paint()
		{
			GL.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit);
			GL.LoadIdentity();

			if(this.kinect != null && this.kinect.IsOpen)
			{
				GL.Enable(EnableCap.Texture2D);
				
				// Swap mid and front RGB
				GCHandle tmp = rgbHandleMid;
				rgbHandleFront = rgbHandleMid;
				rgbHandleMid = tmp;
				
				// Swap mid and front depth
				tmp = depthHandleMid;
				depthHandleFront = depthHandleMid;
				depthHandleMid = tmp;
				
				// Draw RGB texture
				GL.BindTexture(TextureTarget.Texture2D, this.rgbTexture);
				if(this.kinect.VideoCamera.DataFormat == VideoCamera.DataFormatOption.RGB)
				{
					GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Three, 640, 480, 0, OpenTK.Graphics.OpenGL.PixelFormat.Rgb, PixelType.UnsignedByte, (byte[])rgbHandleFront.Target);
				}
				else
				{
					GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.One, 640, 480, 0, OpenTK.Graphics.OpenGL.PixelFormat.Luminance, PixelType.UnsignedByte, (byte[])rgbHandleFront.Target);
				}
				GL.Begin(BeginMode.TriangleFan);
				GL.Color4(255.0f, 255.0f, 255.0f, 255.0f);
				GL.TexCoord2(0, 0); GL.Vertex3(0,0,0);
				GL.TexCoord2(1, 0); GL.Vertex3(640,0,0);
				GL.TexCoord2(1, 1); GL.Vertex3(640,480,0);
				GL.TexCoord2(0, 1); GL.Vertex3(0,480,0);
				GL.End();
				
				// Draw Depth texture
				GL.BindTexture(TextureTarget.Texture2D, this.depthTexture);
				GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Three, 640, 480, 0, OpenTK.Graphics.OpenGL.PixelFormat.Rgb, PixelType.UnsignedByte, (byte[])depthHandleFront.Target);
				GL.Begin(BeginMode.TriangleFan);
				GL.Color4(255.0f, 255.0f, 255.0f, 255.0f);
				GL.TexCoord2(0, 0); GL.Vertex3(640,0,0);
				GL.TexCoord2(1, 0); GL.Vertex3(1280,0,0);
				GL.TexCoord2(1, 1); GL.Vertex3(1280,480,0);
				GL.TexCoord2(0, 1); GL.Vertex3(640,480,0);
				GL.End();
			}
			
			this.imagePanel.SwapBuffers();
		}
		
		/// <summary>
		/// Initialize GUI Elements
		/// </summary>
		private void InitializeComponents()
		{
			this.SuspendLayout();
			
			//
			// headingFont
			//
			this.headingFont = new Font(this.Font.Name, 11, FontStyle.Bold);
			
			//
			// regularFont
			//
			this.regularFont = new Font(this.Font.Name, 9);
			
			//
			// kinectDeviceSelectCombo
			//
			this.kinectDeviceSelectCombo = new ToolStripComboBox();
			this.kinectDeviceSelectCombo.DropDownStyle = ComboBoxStyle.DropDownList;
			this.kinectDeviceSelectCombo.Width = 200;
			
			//
			// connectButton
			//
			this.connectButton = new ToolStripButton();
			this.connectButton.Text = "Connect";
			this.connectButton.Width = 150;
			this.connectButton.Click += delegate(object sender, EventArgs e) {
				this.Connect();
			};
			
			//
			// disconnectButton
			//
			this.disconnectButton = new ToolStripButton();
			this.disconnectButton.Text = "Disconnect";
			this.disconnectButton.Width = 150;
			this.disconnectButton.Enabled = false;
			this.disconnectButton.Click += delegate(object sender, EventArgs e) {
				this.Disconnect();
			};
			
			//
			// mainToolbar
			//
			this.mainToolbar = new ToolStrip();
			this.mainToolbar.Dock = DockStyle.Top;
			this.mainToolbar.Items.Add(this.kinectDeviceSelectCombo);
			this.mainToolbar.Items.Add(this.connectButton);
			this.mainToolbar.Items.Add(this.disconnectButton);
			this.mainToolbar.AutoSize = false;
			this.mainToolbar.Height = 30;
			this.mainToolbar.Renderer = new ToolStripSystemRenderer();
			this.mainToolbar.Enabled = false;
			
			//
			// rgbPanel
			//
			this.rgbPanel = new PictureBox();
			this.rgbPanel.Width = 400;
			this.rgbPanel.Height = 300;
			this.rgbPanel.Margin = new Padding(0);
			this.rgbPanel.SizeMode = PictureBoxSizeMode.StretchImage;
			this.rgbPanel.Click += delegate(object sender, EventArgs e) {
				if(this.kinect != null)
				{
					if(this.kinect.VideoCamera.IsRunning)
					{
						this.kinect.VideoCamera.Stop();	
					}
					else
					{
						this.kinect.VideoCamera.Start();	
					}
				}
			};
			
			//
			// depthPanel
			//
			this.depthPanel = new PictureBox();
			this.depthPanel.Width = 400;
			this.depthPanel.Height = 300;
			this.depthPanel.Margin = new Padding(0);
			this.depthPanel.SizeMode = PictureBoxSizeMode.StretchImage;
			this.depthPanel.Click += delegate(object sender, EventArgs e) {
				if(this.kinect != null)
				{
					if(this.kinect.DepthCamera.IsRunning)
					{
						this.kinect.DepthCamera.Stop();	
					}
					else
					{
						this.kinect.DepthCamera.Start();	
					}
				}
			};
			
			//
			// topPanel
			//
			this.topPanel = new TableLayoutPanel();
			this.topPanel.ColumnCount = 2;
			this.topPanel.RowCount = 1;
			this.topPanel.Controls.Add(this.rgbPanel, 0, 0);
			this.topPanel.Controls.Add(this.depthPanel, 1, 0);
			this.topPanel.AutoSize = true;
			this.topPanel.AutoSizeMode = AutoSizeMode.GrowAndShrink;
			this.topPanel.Margin = new Padding(0);
			
			//
			// imagePanel
			//
			this.imagePanel = new GLControl();
			this.imagePanel.Width = 1280;
			this.imagePanel.Height = 480;
			this.imagePanel.Load += delegate(object sender, EventArgs e) {
				this.ImagePanel_Load();
			};
			this.imagePanel.Paint += delegate(object sender, PaintEventArgs e) {
				this.ImagePanel_Paint();
			};
			
			//
			// ledControlLabel
			//
			this.ledControlLabel = new Label();
			this.ledControlLabel.Text = "LED Color:";
			this.ledControlLabel.AutoSize = true;
			this.ledControlLabel.Dock = DockStyle.Top;
			this.ledControlLabel.Font = this.headingFont;
			this.ledControlLabel.BorderStyle = BorderStyle.None;
			
			//
			// ledControlCombo
			//
			this.ledControlCombo = new ComboBox();
			this.ledControlCombo.DropDownStyle = ComboBoxStyle.DropDownList;
			foreach(string color in Enum.GetNames(typeof(LED.ColorOption)))
			{
				this.ledControlCombo.Items.Add(color);
			}
			this.ledControlCombo.Dock = DockStyle.Top;
			this.ledControlCombo.Font = this.regularFont;
			this.ledControlCombo.SelectedIndexChanged += delegate(object sender, EventArgs e) {
				this.SetLED(this.ledControlCombo.SelectedItem.ToString());
			};
			
			//
			// motorControlLabel
			//
			this.motorControlLabel = new Label();
			this.motorControlLabel.Text = "Motor Tilt Angle:";
			this.motorControlLabel.AutoSize = true;
			this.motorControlLabel.Dock = DockStyle.Top;
			this.motorControlLabel.Font = this.headingFont;
			this.motorControlLabel.BorderStyle = BorderStyle.None;
			
			//
			// motorControlTrack
			//
			this.motorControlTrack = new TrackBar();
			this.motorControlTrack.Minimum = -100;
			this.motorControlTrack.Maximum = 100;
			this.motorControlTrack.Dock = DockStyle.Top;
			this.motorControlTrack.AutoSize = true;
			this.motorControlTrack.TickStyle = TickStyle.TopLeft;
			this.motorControlTrack.SmallChange = 1;
			this.motorControlTrack.LargeChange = 5;
			this.motorControlTrack.ValueChanged += delegate(object sender, EventArgs e) {
				this.SetMotorTilt(this.motorControlTrack.Value);
			};
			
			//
			// videoFormatControlLabel
			//
			this.videoFormatControlLabel = new Label();
			this.videoFormatControlLabel.Text = "Video Format:";
			this.videoFormatControlLabel.AutoSize = true;
			this.videoFormatControlLabel.Dock = DockStyle.Top;
			this.videoFormatControlLabel.Font = this.headingFont;
			this.videoFormatControlLabel.BorderStyle = BorderStyle.None;
			
			//
			// videoFormatControlCombo
			//
			this.videoFormatControlCombo = new ComboBox();
			this.videoFormatControlCombo.DropDownStyle = ComboBoxStyle.DropDownList;
			this.videoFormatControlCombo.Items.Add("RGB");
			this.videoFormatControlCombo.Items.Add("IR");
			this.videoFormatControlCombo.SelectedIndex = 0;
			this.videoFormatControlCombo.Dock = DockStyle.Top;
			this.videoFormatControlCombo.Font = this.regularFont;
			this.videoFormatControlCombo.SelectedIndexChanged += delegate(object sender, EventArgs e) {
				this.kinect.VideoCamera.Stop();
				if(this.videoFormatControlCombo.SelectedIndex == 0)
				{
					this.kinect.VideoCamera.DataFormat = VideoCamera.DataFormatOption.RGB;
				}
				else
				{
					this.kinect.VideoCamera.DataFormat = VideoCamera.DataFormatOption.IR8Bit;
				}
				this.kinect.VideoCamera.Start();
			};
			
			//
			// controlsPanel
			//
			this.controlsPanel = new Panel();
			this.controlsPanel.Width = 300;
			this.controlsPanel.Height = 170;
			this.controlsPanel.Margin = new Padding(0);
			this.controlsPanel.Padding = new Padding(7);
			this.controlsPanel.Controls.Add(this.motorControlTrack);
			this.controlsPanel.Controls.Add(this.motorControlLabel);
			this.controlsPanel.Controls.Add(this.ledControlCombo);
			this.controlsPanel.Controls.Add(this.ledControlLabel);
			this.controlsPanel.Controls.Add(this.videoFormatControlCombo);
			this.controlsPanel.Controls.Add(this.videoFormatControlLabel);
			
			//
			// motorTiltAngleLabel
			//
			this.motorTiltAngleLabel = new Label();
			this.motorTiltAngleLabel.Text = "Tilt Angle: ";
			this.motorTiltAngleLabel.AutoSize = true;
			this.motorTiltAngleLabel.Dock = DockStyle.Top;
			this.motorTiltAngleLabel.Font = this.headingFont;
			this.motorTiltAngleLabel.BorderStyle = BorderStyle.None;
			
			//
			// motorTiltStatusLabel
			//
			this.motorTiltStatusLabel = new Label();
			this.motorTiltStatusLabel.Text = "Tilt Status: ";
			this.motorTiltStatusLabel.AutoSize = true;
			this.motorTiltStatusLabel.Dock = DockStyle.Top;
			this.motorTiltStatusLabel.Font = this.headingFont;
			this.motorTiltStatusLabel.BorderStyle = BorderStyle.None;
			
			//
			// accelXLabel
			//
			this.accelXLabel = new Label();
			this.accelXLabel.Text = "Accel X: ";
			this.accelXLabel.AutoSize = true;
			this.accelXLabel.Dock = DockStyle.Top;
			this.accelXLabel.Font = this.headingFont;
			this.accelXLabel.BorderStyle = BorderStyle.None;
			
			//
			// accelYLabel
			//
			this.accelYLabel = new Label();
			this.accelYLabel.Text = "Accel Y: ";
			this.accelYLabel.AutoSize = true;
			this.accelYLabel.Dock = DockStyle.Top;
			this.accelYLabel.Font = this.headingFont;
			this.accelYLabel.BorderStyle = BorderStyle.None;
			
			//
			// accelZLabel
			//
			this.accelZLabel = new Label();
			this.accelZLabel.Text = "Accel Z: ";
			this.accelZLabel.AutoSize = true;
			this.accelZLabel.Dock = DockStyle.Top;
			this.accelZLabel.Font = this.headingFont;
			this.accelZLabel.BorderStyle = BorderStyle.None;
			
			//
			// infoPanel
			//
			this.infoPanel = new TableLayoutPanel();
			this.infoPanel.ColumnCount = 2;
			this.infoPanel.RowCount = 3;
			this.infoPanel.Width = 500;
			this.infoPanel.Height = 120;
			this.infoPanel.Margin = new Padding(0);
			this.infoPanel.Padding = new Padding(7);
			this.infoPanel.Controls.Add(this.accelZLabel, 1, 2);
			this.infoPanel.Controls.Add(this.accelYLabel, 1, 1);
			this.infoPanel.Controls.Add(this.accelXLabel, 1, 0);
			this.infoPanel.Controls.Add(this.motorTiltAngleLabel, 0, 0);
			this.infoPanel.Controls.Add(this.motorTiltStatusLabel, 0, 0);
			this.infoPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50.0f));
			this.infoPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50.0f));
			
			//
			// debugTextbox
			//
			this.debugTextbox = new TextBox();
			this.debugTextbox.Multiline = true;
			this.debugTextbox.Dock = DockStyle.Fill;
			this.debugTextbox.ScrollBars = ScrollBars.Vertical;
			this.debugTextbox.BackColor = Color.Aqua;
			
			//
			// bottomPanel
			//
			this.bottomPanel = new TableLayoutPanel();
			this.bottomPanel.BackColor = Color.White;
			this.bottomPanel.ColumnCount = 2;
			this.bottomPanel.RowCount = 2;
			this.bottomPanel.Controls.Add(this.infoPanel, 1, 0);
			this.bottomPanel.Controls.Add(this.controlsPanel, 0, 0);
			this.bottomPanel.AutoSize = true;
			this.bottomPanel.AutoSizeMode = AutoSizeMode.GrowAndShrink;
			this.bottomPanel.Margin = new Padding(0);
			this.bottomPanel.Enabled = false;
			
			//
			// mainLayoutPanel
			//
			this.mainLayoutPanel = new TableLayoutPanel();
			this.mainLayoutPanel.ColumnCount = 1;
			this.mainLayoutPanel.RowCount = 2;
			this.mainLayoutPanel.Controls.Add(this.imagePanel, 0, 0);
			this.mainLayoutPanel.Controls.Add(this.bottomPanel, 0, 1);
			this.mainLayoutPanel.AutoSize = true;
			this.mainLayoutPanel.AutoSizeMode = AutoSizeMode.GrowAndShrink;
			this.mainLayoutPanel.Margin = new Padding(0);
			this.mainLayoutPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize, 0));
			this.mainLayoutPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize, 0));
			this.mainLayoutPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 100));
			
			//
			// infoUpdateTimer
			//
			this.infoUpdateTimer = new System.Windows.Forms.Timer();
			this.infoUpdateTimer.Interval = 500;
			this.infoUpdateTimer.Enabled = false;
			this.infoUpdateTimer.Tick += delegate(object sender, EventArgs e) {
				this.UpdateInfoPanel();
			};
			
			//
			// MainForm
			//
			this.FormBorderStyle = FormBorderStyle.Fixed3D;
			this.Text = "Kinect.NET Wrapper Testing Interface";
			this.AutoSize = true;
			this.AutoSizeMode = AutoSizeMode.GrowAndShrink;
			this.BackColor = Color.White;
			this.Controls.Add(this.mainToolbar);
			this.Controls.Add(this.mainLayoutPanel);
			this.FormClosing += delegate(object sender, FormClosingEventArgs e) {
				this.rgbHandleFront.Free();
				this.rgbHandleMid.Free();
				this.rgbHandleBack.Free();
				Kinect.Shutdown();
			};
			
			// Update
			this.ResumeLayout(false);
			this.PerformLayout();
		}		
		
		// GUI Elements
		private ToolStrip mainToolbar;
		private ToolStripComboBox kinectDeviceSelectCombo;
		private ToolStripButton connectButton;
		private ToolStripButton disconnectButton;
		private TableLayoutPanel topPanel;
		private PictureBox rgbPanel;
		private PictureBox depthPanel;
		private GLControl imagePanel;
		private TableLayoutPanel bottomPanel;
		private Panel controlsPanel;
		private Label ledControlLabel;
		private ComboBox ledControlCombo;
		private Label motorControlLabel;
		private TrackBar motorControlTrack;
		private Label videoFormatControlLabel;
		private ComboBox videoFormatControlCombo;
		private TableLayoutPanel infoPanel;
		private Label motorTiltAngleLabel;
		private Label motorTiltStatusLabel;
		private Label accelXLabel;
		private Label accelYLabel;
		private Label accelZLabel;
		private TextBox debugTextbox;
		private TableLayoutPanel mainLayoutPanel;
		private Font headingFont;
		private Font regularFont;
		private System.Windows.Forms.Timer infoUpdateTimer;
	}
}