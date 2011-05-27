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
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;
using freenect;
using OpenTK;
using OpenTK.Graphics.OpenGL;
using System.Drawing;
using System.Threading;

namespace KinectDemo
{
	/// <summary>
	/// Displays video / depth data
	/// </summary>
	public partial class PreviewControl : UserControl
	{
		
		/// <summary>
		/// Gets or sets the current frame mode for the video feed
		/// </summary>
		public VideoFrameMode VideoMode
		{
			get
			{
				return this.videoMode;
			}
			set
			{
				// Make sure we are actually changing something
				if(this.videoMode == value)
				{
					return;
				}
				
				// Resize buffer
				if(this.videoDataBuffers != null)
				{
					this.videoDataBuffers.Dispose();	
				}
				this.videoDataBuffers = new SwapBufferCollection<byte>(3, 3 * value.Width * value.Height);
				
				// Save mode
				this.videoMode = value;
			}
		}
		
		/// <summary>
		/// Gets or sets the current frame mode for the depth feed
		/// </summary>
		public DepthFrameMode DepthMode
		{
			get
			{
				return this.depthMode;
			}
			set
			{
				// Make sure we are actually changing something
				if(this.depthMode == value)
				{
					return;
				}
				
				// Resize buffer
				if(this.depthDataBuffers != null)
				{
					this.depthDataBuffers.Dispose();	
				}
				this.depthDataBuffers = new SwapBufferCollection<byte>(3, 3 * value.Width * value.Height);
				
				// Save mode
				this.depthMode = value;
			}
		}
		
		/// <summary>
		/// Handle to back buffer for the video feed
		/// </summary>
		public IntPtr VideoBackBuffer
		{	
			get
			{
				return this.videoDataBuffers.GetHandle(2);
			}
		}
		
		/// <summary>
		/// Handle to back buffer for the depth feed
		/// </summary>
		public IntPtr DepthBackBuffer
		{	
			get
			{
				return this.depthDataBuffers.GetHandle(2);
			}
		}
		
		/// <summary>
		/// Gets the FPS for the depth feed
		/// </summary>
		public int DepthFPS
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the FPS for the video feed
		/// </summary>
		public int VideoFPS
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Video mode
		/// </summary>
		private VideoFrameMode videoMode = null;
		
		/// <summary>
		/// Depth mode
		/// </summary>
		private DepthFrameMode depthMode = null;
		
		/// <summary>
		/// Texture for depth data
		/// </summary>
		private uint depthTexture;
		
		/// <summary>
		/// Texture for video data
		/// </summary>
		private uint videoTexture;
		
		/// <summary>
		/// Swappable data buffers for depth data
		/// </summary>
		private SwapBufferCollection<byte> depthDataBuffers;
		
		/// <summary>
		/// Swappable data buffers for video data
		/// </summary>
		private SwapBufferCollection<byte> videoDataBuffers;
		
		/// <summary>
		/// Is new video data pending?
		/// </summary>
		private bool videoDataPending = false;
		
		/// <summary>
		/// Is new depth data pending?
		/// </summary>
		private bool depthDataPending = false;
		
		// Gamma constants for color map generation
		private UInt16[] gamma = new UInt16[2048];
		
		/// <summary>
		/// Last time FPS was updated for Depth
		/// </summary>
		private DateTime lastDepthFPSUpate = DateTime.Now;
		
		/// <summary>
		/// Number of frames since last FPS update for depth
		/// </summary>
		private int depthFrameCount = 0;
		
		/// <summary>
		/// Last time FPS was updated for video
		/// </summary>
		private DateTime lastVideoFPSUpdate = DateTime.Now;
		
		/// <summary>
		/// Number of frames since last FPS update for video
		/// </summary>
		private int videoFrameCount = 0;
		
		/// <summary>
		/// Constructor
		/// </summary>
		public PreviewControl()
		{
			// Initialize UI components
			this.InitializeComponents();
			
			// Caluclate gamma constants
			for (int i = 0 ; i < 2048; i++) 
			{
				double v = i / 2048.0;
				v = Math.Pow((double)v, 3.0) * 6.0;
				gamma[i] = (UInt16)(v * 6.0 * 256.0);
			}
		}
		
		/// <summary>
		/// Handle video data being updated
		/// </summary>
		public void HandleVideoBackBufferUpdate()
		{
			// Swap middle and back buffers
			if(this.VideoMode.Format == VideoFormat.Infrared10Bit)
			{
				// Swap mid and back
				unsafe
				{
					Int16 *ptrMid 	= (Int16 *)this.videoDataBuffers.GetHandle(1);
					Int16 *ptrBack 	= (Int16 *)this.videoDataBuffers.GetHandle(2);
					int dim 		= this.VideoMode.Width * this.VideoMode.Height;
					int i 			= 0;
					Int16 mult		= 50;
					
					for (i = 0; i < dim; i++)
					{
						*ptrMid++ = (Int16)(ptrBack[i] * mult);
					}
				}
			}
			else
			{
				this.videoDataBuffers.Swap(1, 2);
			}
			
			// Calculate FPS
			this.videoFrameCount++;
			if((DateTime.Now - this.lastVideoFPSUpdate).Seconds >= 1)
			{
				this.VideoFPS = this.videoFrameCount;
				this.videoFrameCount = 0;
				this.lastVideoFPSUpdate = DateTime.Now;
			}
			
			// New data!
			this.videoDataPending = true;
		}
		
		/// <summary>
		/// Handle depth data being updated
		/// </summary>
		public void HandleDepthBackBufferUpdate()
		{
			// Swap mid and back
			unsafe
			{
				byte *ptrMid 	= (byte *)this.depthDataBuffers.GetHandle(1);
				Int16 *ptrBack 	= (Int16 *)this.depthDataBuffers.GetHandle(2);
				int dim 		= this.DepthMode.Width * this.DepthMode.Height;
				int i 			= 0;
				Int16 pval		= 0;
				Int16 lb 		= 0;
				for (i = 0; i < dim; i++)
				{
					pval 	= (Int16)this.gamma[ptrBack[i]];
					lb 		= (Int16)(pval & 0xff);
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
			
			// Calculate FPS
			this.depthFrameCount++;
			if((DateTime.Now - this.lastDepthFPSUpate).Seconds >= 1)
			{
				this.DepthFPS = this.depthFrameCount;
				this.depthFrameCount = 0;
				this.lastDepthFPSUpate = DateTime.Now;
			}
			
			// New data!
			this.depthDataPending = true;
		}
		
		/// <summary>
		/// Make the preview control render a frame
		/// </summary>
		public void Render()
		{
			this.renderPanel.Invalidate();
		}
		
		/// <summary>
		/// Handle render panel OnPaint
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="PaintEventArgs"/>
		/// </param>
		private void HandleRenderPanelPaint(object sender, PaintEventArgs e)
		{
			// Init GL window for render
			GL.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit);
			GL.LoadIdentity();
			GL.ClearColor(Color.Blue);
			GL.Enable(EnableCap.Texture2D);
			
			/// Render video
			this.HandleRenderPanelPaintVideo();
			
			// Render depth
			this.HandleRenderPanelPaintDepth();
			
			// Present
			this.renderPanel.SwapBuffers();
		}

		/// <summary>
		/// Render video feed
		/// </summary>
		private void HandleRenderPanelPaintVideo()
		{
			if(this.VideoMode == null)
			{
				// Not rendering anything
				return;
			}
			
			if(this.videoDataPending)
			{
				// Swap middle and front buffers (we will be rendering off of front buffer)
				this.videoDataBuffers.Swap(0, 1);
			}
			
			// Use preview texture for rendering
			GL.BindTexture(TextureTarget.Texture2D, this.videoTexture);
			
			// Setup texture
			VideoFormat format = this.VideoMode.Format;
			switch(format)
			{
				case VideoFormat.RGB:
					GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Three, this.VideoMode.Width, this.VideoMode.Height, 0, OpenTK.Graphics.OpenGL.PixelFormat.Rgb, PixelType.UnsignedByte, this.videoDataBuffers.GetHandle(0));
					break;
				case VideoFormat.Infrared8Bit:
					GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.One, this.VideoMode.Width, this.VideoMode.Height, 0, OpenTK.Graphics.OpenGL.PixelFormat.Luminance, PixelType.UnsignedByte, this.videoDataBuffers.GetHandle(0));
					break;
				case VideoFormat.Infrared10Bit:
					GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Luminance16, this.VideoMode.Width, this.VideoMode.Height, 0, OpenTK.Graphics.OpenGL.PixelFormat.Luminance, PixelType.UnsignedShort, this.videoDataBuffers.GetHandle(0));
					break;
			}
			
			// Draw texture
			GL.Begin(BeginMode.TriangleFan);
			GL.Color4(255.0f, 255.0f, 255.0f, 255.0f);
			GL.TexCoord2(0, 0); GL.Vertex3(0, 0, 0);
			GL.TexCoord2(1, 0); GL.Vertex3(640, 0, 0);
			GL.TexCoord2(1, 1); GL.Vertex3(640, 480, 0);
			GL.TexCoord2(0, 1); GL.Vertex3(0, 480, 0);
			GL.End();
			
			// Video data not pending anymore
			this.videoDataPending = false;
		}
		
		/// <summary>
		/// Render depth feed
		/// </summary>
		private void HandleRenderPanelPaintDepth()
		{
			if(this.DepthMode == null)
			{
				// Not rendering anything
				return;
			}
			
			if(this.depthDataPending)
			{
				// Swap front and middle buffers
				this.depthDataBuffers.Swap(0, 1);
			}
			
			// Use preview texture for rendering
			GL.BindTexture(TextureTarget.Texture2D, this.depthTexture);
			
			// Setup texture
			GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Three, this.DepthMode.Width, this.DepthMode.Height, 0, OpenTK.Graphics.OpenGL.PixelFormat.Rgb, PixelType.UnsignedByte, this.depthDataBuffers.GetHandle(0));
			
			// Draw texture
			GL.Begin(BeginMode.TriangleFan);
			GL.Color4(255.0f, 255.0f, 255.0f, 255.0f);
			GL.TexCoord2(0, 0); GL.Vertex3(640, 0, 0);
			GL.TexCoord2(1, 0); GL.Vertex3(1280, 0, 0);
			GL.TexCoord2(1, 1); GL.Vertex3(1280, 480, 0);
			GL.TexCoord2(0, 1); GL.Vertex3(640, 480, 0);
			GL.End();
			
			// Depth data handled
			this.depthDataPending = false;
		}

		/// <summary>
		/// Handle render panel on load
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="EventArgs"/>
		/// </param>
		private void HandleRenderPanelLoad(object sender, EventArgs e)
		{
			GL.ClearColor(Color.Blue);
			GL.ClearDepth(1.0);
			GL.DepthFunc(DepthFunction.Less);
			GL.Disable(EnableCap.DepthTest);
			GL.Enable(EnableCap.Blend);
			GL.BlendFunc(BlendingFactorSrc.SrcAlpha, BlendingFactorDest.OneMinusSrcAlpha);
			GL.ShadeModel(ShadingModel.Smooth);

			GL.GenTextures(1, out this.videoTexture);
			GL.BindTexture(TextureTarget.Texture2D, this.videoTexture);
			GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Linear);
			GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter, (int)TextureMinFilter.Linear);
			
			GL.GenTextures(1, out this.depthTexture);
			GL.BindTexture(TextureTarget.Texture2D, this.depthTexture);
			GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Linear);
			GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter, (int)TextureMinFilter.Linear);
			
			GL.Viewport(0, 0, 1280, 480);
			GL.MatrixMode(MatrixMode.Projection);
			GL.LoadIdentity();
			GL.Ortho(0, 1280, 480, 0, -1.0f, 1.0f);
			GL.MatrixMode(MatrixMode.Modelview);
		}
	}
}