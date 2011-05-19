using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;
using freenect;
using OpenTK;
using OpenTK.Graphics.OpenGL;
using System.Drawing;

namespace KinectDemo
{
	/// <summary>
	/// Displays video / depth data
	/// </summary>
	public partial class PreviewControl : UserControl
	{
		
		/// <summary>
		/// Initialize UI components
		/// </summary>
		public void InitializeComponents()
		{
			///
			/// renderPanel
			/// 
			this.renderPanel = new OpenTK.GLControl();
			this.renderPanel.Dock = DockStyle.Fill;
			this.renderPanel.Load += HandleRenderPanelLoad;
			this.renderPanel.Paint += HandleRenderPanelPaint;
			
			///
			/// PreviewWindow
			/// 
			this.BackColor = Color.Blue;
			this.Controls.Add(this.renderPanel);
		}
		
		///
		/// UI Components
		/// 
		protected OpenTK.GLControl renderPanel;
		
	}
}