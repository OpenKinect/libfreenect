using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;
using System.Drawing;

namespace KinectDemo
{
	public partial class PreviewWindow : Form
	{
		/// <summary>
		/// Initialize UI components
		/// </summary>
		private void InitializeComponents()
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
			this.BackColor = Color.White;
			this.FormBorderStyle = FormBorderStyle.FixedSingle;
			this.MaximizeBox = false;
			this.Controls.Add(this.renderPanel);
		}
		
		///
		/// UI Components
		/// 
		protected OpenTK.GLControl renderPanel;
		
	}
}