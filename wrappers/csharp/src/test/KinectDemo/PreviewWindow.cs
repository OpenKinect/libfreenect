using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;
using freenect;
using OpenTK;
using OpenTK.Graphics.OpenGL;
using System.Drawing;
using System.Runtime.InteropServices;

namespace KinectDemo
{
	public abstract partial class PreviewWindow : Form
	{
		
		/// <summary>
		/// Gets the handle to the current back buffer for the preview window
		/// </summary>
		public IntPtr BackBuffer
		{
			get
			{
				return this.previewDataBuffers.GetHandle(2);
			}
		}
		
		/// <summary>
		/// Gets whether the preview window is busy rendering something
		/// </summary>
		public bool IsBusy
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the current frame mode
		/// </summary>
		public FrameMode Mode
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets FPS for the preview window
		/// </summary>
		public uint FPS
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Swappable data buffers
		/// </summary>
		protected SwapBufferCollection<byte> previewDataBuffers;
		
		/// <summary>
		/// Texture for preview data
		/// </summary>
		protected uint previewTexture;
		
		/// <summary>
		/// Did new data come in?
		/// </summary>
		protected bool newDataPending = false;
		
		/// <summary>
		/// Frames since last FPS update
		/// </summary>
		private uint fpsCount = 0;
		
		/// <summary>
		/// Last time FPS was updated
		/// </summary>
		private DateTime lastFPSUpdate = DateTime.Now;
		
		/// <summary>
		/// Constructor
		/// </summary>
		public PreviewWindow()
		{
			// Initialize UI
			this.InitializeComponents();
		}
		
		/// <summary>
		/// Resize the preview window for the frame mode specified. This should
		/// setup buffers and OpenGL rendering surfaces accordingly
		/// </summary>
		/// <param name="mode">
		/// A <see cref="FrameMode"/>
		/// </param>
		public new void Resize(FrameMode mode)
		{
			// Resize scene
			GL.Viewport(0, 0, mode.Width, mode.Height);
			GL.MatrixMode(MatrixMode.Projection);
			GL.LoadIdentity();
			GL.Ortho(0, mode.Width, mode.Height, 0, -1.0f, 1.0f);
			GL.MatrixMode(MatrixMode.Modelview);
			
			// Resize buffers
			if(this.previewDataBuffers != null)
			{
				this.previewDataBuffers.Dispose();
			}
			this.previewDataBuffers = new SwapBufferCollection<byte>(3, mode.Width * mode.Height * 3);
			
			// Resize window
			this.Width = mode.Width;
			this.Height = mode.Height;
			
			// Save frame mode
			this.Mode = mode;
		}
		
		/// <summary>
		/// Handle new data in the back buffer
		/// </summary>
		public abstract void HandleBackBufferUpdate();
		
		/// <summary>
		/// Render current frame of data
		/// </summary>
		public abstract void RenderPreview();		
		
		/// <summary>
		/// Handle render panel OnPaint
		/// </summary>
		/// <param name="sender">
		/// A <see cref="System.Object"/>
		/// </param>
		/// <param name="e">
		/// A <see cref="PaintEventArgs"/>
		/// </param>
		private void HandleRenderPanelPaint (object sender, PaintEventArgs e)
		{
			// Init GL window for render
			GL.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit);
			GL.LoadIdentity();
			GL.ClearColor(Color.Blue);
			GL.Enable(EnableCap.Texture2D);
			
			// Do we have new data?
			if(this.newDataPending)
			{
				// Use preview texture for rendering
				GL.BindTexture(TextureTarget.Texture2D, this.previewTexture);
				
				// Yes! Render
				this.IsBusy = true;
				this.RenderPreview();
				this.IsBusy = false;
				
				// Draw texture
				GL.Begin(BeginMode.TriangleFan);
				GL.Color4(255.0f, 255.0f, 255.0f, 255.0f);
				GL.TexCoord2(0, 0); GL.Vertex3(0, 0, 0);
				GL.TexCoord2(1, 0); GL.Vertex3(this.Mode.Width, 0, 0);
				GL.TexCoord2(1, 1); GL.Vertex3(this.Mode.Width, this.Mode.Height, 0);
				GL.TexCoord2(0, 1); GL.Vertex3(0, this.Mode.Height, 0);
				GL.End();
				
				// No more new data
				this.newDataPending = false;
			}
			
			// Calculate FPS
			this.fpsCount++;
			if((DateTime.Now - this.lastFPSUpdate).Seconds >= 1)
			{
				this.FPS = this.fpsCount;
				this.fpsCount = 0;
				
				if(this.Mode is VideoFrameMode)
				{
					VideoFrameMode mode = (VideoFrameMode)this.Mode;
					this.Text = mode.Format.ToString() + " - " + mode.Width + "x" + mode.Height + " FPS:" + this.FPS;
				}
				else if(this.Mode is DepthFrameMode)
				{
					DepthFrameMode mode = (DepthFrameMode)this.Mode;
					this.Text = mode.Format.ToString() + " - " + mode.Width + "x" + mode.Height + " FPS:" + this.FPS;
				}
				
				this.lastFPSUpdate = DateTime.Now;
			}
			
			// Present
			this.renderPanel.SwapBuffers();
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
		private void HandleRenderPanelLoad (object sender, EventArgs e)
		{
			GL.ClearColor(Color.Blue);
			GL.ClearDepth(1.0);
			GL.DepthFunc(DepthFunction.Less);
			GL.Disable(EnableCap.DepthTest);
			GL.Enable(EnableCap.Blend);
			GL.BlendFunc(BlendingFactorSrc.SrcAlpha, BlendingFactorDest.OneMinusSrcAlpha);
			GL.ShadeModel(ShadingModel.Smooth);

			GL.GenTextures(1, out this.previewTexture);
			GL.BindTexture(TextureTarget.Texture2D, this.previewTexture);
			GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Linear);
			GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter, (int)TextureMinFilter.Linear);
		}
		
	}
}