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
	public partial class VideoPreviewWindow : PreviewWindow
	{
	
		/// <summary>
		/// Handle new data in the back buffer
		/// </summary>
		public override void HandleBackBufferUpdate()
		{
			try
			{
				// Swap middle and back buffers
				this.previewDataBuffers.Swap(1, 2);
				
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
		/// Render data
		/// </summary>
		public override void RenderPreview()
		{
			// Swap middle and front buffers (we will be rendering off of front buffer)
			this.previewDataBuffers.Swap(0, 1);
			
			// Setup texture
			VideoFormat format = ((VideoFrameMode)this.Mode).Format;
			switch(format)
			{
				case VideoFormat.RGB:
					GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Three, this.Mode.Width, this.Mode.Height, 0, OpenTK.Graphics.OpenGL.PixelFormat.Rgb, PixelType.UnsignedByte, this.previewDataBuffers[0]);
					break;
				case VideoFormat.Infrared8Bit:
					GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.One, this.Mode.Width, this.Mode.Height, 0, OpenTK.Graphics.OpenGL.PixelFormat.Luminance, PixelType.UnsignedByte, this.previewDataBuffers[0]);
					break;
			}
		}
		
	}
}