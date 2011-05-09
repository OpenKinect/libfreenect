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
		
		// Gamma constants for color map generation
		private UInt16[] gamma = new UInt16[2048];
		
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
			
			// Color map rendering by default
			this.PreviewMode = DepthPreviewWindow.DepthPreviewMode.ColorMap;	
		}
		
		/// <summary>
		/// Handle new back buffer data
		/// </summary>
		public override void HandleBackBufferUpdate()
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
		
		/// <summary>
		/// Translates new back buffer data into the middle buffer as a color map
		/// </summary>
		public void HandleBackBufferUpdateColorMap()
		{
			unsafe
			{
				byte *ptrMid 	= (byte *)this.previewDataBuffers.GetHandle(1);
				Int16 *ptrBack 	= (Int16 *)this.previewDataBuffers.GetHandle(2);
				int dim 		= this.Mode.Width * this.Mode.Height;
				int i 			= 0;
				
				for (i = 0; i < dim; i++)
				{
					*ptrMid++ = 255;
					*ptrMid++ = 0;
					*ptrMid++ = 0;
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
			}
		}
		/// <summary>
		/// Renders the preview data as a color map
		/// </summary>
		private void RenderColorMap()
		{
			GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Three, this.Mode.Width, this.Mode.Height, 0, OpenTK.Graphics.OpenGL.PixelFormat.Rgb, PixelType.UnsignedByte, this.previewDataBuffers[0]);
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