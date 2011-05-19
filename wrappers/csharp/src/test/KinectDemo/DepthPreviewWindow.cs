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
	public partial class DepthPreviewWindow : PreviewWindow
	{
		
		/// <summary>
		/// Gets or sets the preview mode for the depth preview window
		/// </summary>
		public DepthPreviewMode PreviewMode
		{
			get;
			set;
		}
		
		/// <summary>
		/// Context menu for the depth window. This lets us change things like the preview mode
		/// </summary>
		private ContextMenuStrip contextMenu;
		
		// Gamma constants for color map generation
		private UInt16[] gamma = new UInt16[2048];
		
		/// <summary>
		/// Point cloud data array
		/// </summary>
		private Vector3[] pointCloudData;
		
		/// <summary>
		/// Vertex buffer for the point cloud
		/// </summary>
		int pointCloudVertexBuffer;
		
		/// <summary>
		/// Projection for point cloud
		/// </summary>
		Matrix4 pointCloudProjection;
		
		/// <summary>
		/// Constructor
		/// </summary>
		public DepthPreviewWindow() : base()
		{
			// Caluclate gamma constants
			for (int i = 0 ; i < 2048; i++) 
			{
				double v = i / 2048.0;
				v = Math.Pow((double)v, 3.0) * 6.0;
				gamma[i] = (UInt16)(v * 6.0 * 256.0);
			}
			
			// Initialize context menu
			this.contextMenu = new ContextMenuStrip();
			ToolStripItem colorMapModeItem = new ToolStripButton("Color Map");
			colorMapModeItem.Click += delegate {
				this.PreviewMode = DepthPreviewWindow.DepthPreviewMode.ColorMap;
				this.Resize(this.Mode);
			};
			ToolStripItem pointCloudModeItem = new ToolStripButton("Point Cloud");
			pointCloudModeItem.Click += delegate {
				this.PreviewMode = DepthPreviewWindow.DepthPreviewMode.PointCloud;
				this.Resize(this.Mode);
			};
			this.contextMenu.Items.Add(colorMapModeItem);
			this.contextMenu.Items.Add(pointCloudModeItem);
			//this.ContextMenuStrip = this.contextMenu;
			
			// Color map rendering by default
			this.PreviewMode = DepthPreviewWindow.DepthPreviewMode.ColorMap;	
		}
		
		/// <summary>
		/// Handle new back buffer data
		/// </summary>
		public override void HandleBackBufferUpdate()
		{
			try
			{
				switch(this.PreviewMode)
				{
					case DepthPreviewMode.ColorMap:
						this.HandleBackBufferUpdateColorMap();
						break;
				}
				
				// Have new data
				this.newDataPending = true;
				
				// Invalid render panel
				this.renderPanel.Invalidate();
			}
			catch(Exception e)
			{
				Console.WriteLine(e.Message);
			}
		}
		
		/// <summary>
		/// Translates new back buffer data for point cloud
		/// </summary>
		private void HandleBackBufferUpdatePointCloud()
		{
			DepthFrameMode mode = this.Mode as DepthFrameMode;
			if(mode.Format == DepthFormat.Depth10Bit || mode.Format == DepthFormat.Depth11Bit)
			{
				this.HandleBackBufferUpdatePointCloudUnpacked();
			}
		}
		
		/// <summary>
		/// Translates back buffer data for unpacked 11bit or 10it data
		/// </summary>
		private void HandleBackBufferUpdatePointCloudUnpacked()
		{
			unsafe
			{
				Int16 *ptrBack 	= (Int16 *)this.previewDataBuffers.GetHandle(2);
				int dim 		= this.Mode.Width * this.Mode.Height;
				for(int i = 0; i < dim; i++)
				{
					this.pointCloudData[i].Z = ptrBack[i];
				}
			}
		}
		
		/// <summary>
		/// Translates new back buffer data into the middle buffer as a color map
		/// </summary>
		private void HandleBackBufferUpdateColorMap()
		{
			DepthFrameMode mode = this.Mode as DepthFrameMode;
			if(mode.Format == DepthFormat.Depth10Bit || mode.Format == DepthFormat.Depth11Bit)
			{
				this.HandleBackBufferUpdateColorMapUnpacked();
			}
		}
		
		/// <summary>
		/// Translates back buffer data for unpacked 11bit or 10it data
		/// </summary>
		private void HandleBackBufferUpdateColorMapUnpacked()
		{
			// Swap mid and back
			unsafe
			{
				byte *ptrMid 	= (byte *)this.previewDataBuffers.GetHandle(1);
				Int16 *ptrBack 	= (Int16 *)this.previewDataBuffers.GetHandle(2);
				int dim 		= this.Mode.Width * this.Mode.Height;
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
		
		/// <summary>
		/// Renders preview data according to PreviewMode
		/// </summary>
		public override void RenderPreview()
		{
			// Swap front and middle buffers
			this.previewDataBuffers.Swap(0, 1);
			
			switch(this.PreviewMode)
			{
				case DepthPreviewMode.ColorMap:
					this.RenderColorMap();
					break;
				case DepthPreviewMode.PointCloud:
					this.RenderPointCloud();
					break;
			}
		}
		
		/// <summary>
		/// Renders the preview data as a point cloud
		/// </summary>
		private void RenderPointCloud()
		{
			GL.Clear(ClearBufferMask.ColorBufferBit |
                     ClearBufferMask.DepthBufferBit|
                     ClearBufferMask.StencilBufferBit);
			
			GL.ClearColor(Color.Black);
			Console.Write("-");
			GL.Color3(Color.White);
   			GL.PointSize(2.0f);
			
			Matrix4 lookat = Matrix4.LookAt(0, 128, 256, 0, 0, 0, 0, 1, 0);
            Vector3 scale = new Vector3(4, 4, 4);
            GL.MatrixMode(MatrixMode.Modelview);
            GL.LoadMatrix(ref lookat);
            GL.Scale(scale);
			
			GL.EnableClientState(ArrayCap.VertexArray);
            GL.BindBuffer(BufferTarget.ArrayBuffer, this.pointCloudVertexBuffer);
            GL.VertexPointer(3, VertexPointerType.Float, Vector3.SizeInBytes, new IntPtr(0));
            GL.DrawArrays(BeginMode.Points, 0, this.pointCloudData.Length);
		}
		
		/// <summary>
		/// Renders the preview data as a color map
		/// </summary>
		private void RenderColorMap()
		{
			// Use preview texture for rendering
			GL.BindTexture(TextureTarget.Texture2D, this.previewTexture);
			
			// Setup texture
			GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Three, this.Mode.Width, this.Mode.Height, 0, OpenTK.Graphics.OpenGL.PixelFormat.Rgb, PixelType.UnsignedByte, this.previewDataBuffers[0]);
			
			// Draw texture
			GL.Begin(BeginMode.TriangleFan);
			GL.Color4(255.0f, 255.0f, 255.0f, 255.0f);
			GL.TexCoord2(0, 0); GL.Vertex3(0, 0, 0);
			GL.TexCoord2(1, 0); GL.Vertex3(this.Mode.Width, 0, 0);
			GL.TexCoord2(1, 1); GL.Vertex3(this.Mode.Width, this.Mode.Height, 0);
			GL.TexCoord2(0, 1); GL.Vertex3(0, this.Mode.Height, 0);
			GL.End();
		}
		
		/// <summary>
		/// Resize stuff for new mode
		/// </summary>
		/// <param name="mode">
		/// A <see cref="FrameMode"/>
		/// </param>
		public override void Resize(FrameMode mode)
		{
			base.Resize(mode);
			
			if(this.PreviewMode == DepthPreviewWindow.DepthPreviewMode.PointCloud)
			{
				// Resize point cloud array
				this.pointCloudData = new Vector3[mode.Width * mode.Height];
				int count = 0;
				for(int y = 0; y < this.Mode.Height; y++)
				{
					for(int x = 0; x < this.Mode.Width; x++)
					{
						this.pointCloudData[count].X = x;
						this.pointCloudData[count].Y = y;
						count++;
					}
				}
				
				// Generate vertex buffer 
				GL.GenBuffers(1, out this.pointCloudVertexBuffer);
				GL.BindBuffer(BufferTarget.ArrayBuffer, this.pointCloudVertexBuffer);
				GL.BufferData(BufferTarget.ArrayBuffer,
								new IntPtr(this.pointCloudData.Length * BlittableValueType.StrideOf(this.pointCloudData)),
								this.pointCloudData, BufferUsageHint.StaticDraw);
				
				// Setup projection matrix
				float aspect_ratio = this.Mode.Width / (float)this.Mode.Height;
	            this.pointCloudProjection = Matrix4.CreatePerspectiveFieldOfView(MathHelper.PiOver4, aspect_ratio, 1, 512);
	            GL.MatrixMode(MatrixMode.Projection);
	            GL.LoadMatrix(ref this.pointCloudProjection);
			}
		}
		
		/// <summary>
		/// Preview mode.
		/// </summary>
		public enum DepthPreviewMode
		{
			ColorMap,
			PointCloud
		}
		
	}
}