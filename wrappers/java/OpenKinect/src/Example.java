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

import java.io.File;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ShortBuffer;
import javax.imageio.ImageIO;
import org.openkinect.Context;
import org.openkinect.Device;
import org.openkinect.Acceleration;
import org.openkinect.Image;
import org.openkinect.LEDStatus;

/**
 * @author Michael Nischt
 */
public class Example
{
    public static void main(String[] args) throws IOException
    {
        final int width = 640, height = 480;
        final BufferedImage color = new BufferedImage(width, height, BufferedImage.TYPE_3BYTE_BGR);
        final BufferedImage depth = new BufferedImage(width, height, BufferedImage.TYPE_3BYTE_BGR);
        //final BufferedImage depth = new BufferedImage(width, height, BufferedImage.TYPE_BYTE_GRAY);


        Context context = Context.getContext();

        if(context.devices() < 1)
        {
            System.out.println("No Kinect devices found.");
            return;
        }

        Device device = context.getDevice(0);

        device.light(LEDStatus.LED_GREEN);
        device.tilt(-5);

        device.depth(new Image()
        {
            public void data(ByteBuffer image)
            {
                ShortBuffer data = image.asShortBuffer();

                for(int y=0; y<height; y++)
                {
                    for(int x=0; x<width; x++)
                    {
                        int offset = (y*width+x);

                        short d = data.get( offset );

                        if(depth.getType() == BufferedImage.TYPE_BYTE_GRAY)
                        {
                            int pixel = depth2intensity(d);
                            depth.setRGB(x, y, pixel);
                        }
                        else
                        {
                            int pixel = depth2rgb(d);
                            depth.setRGB(x, y, pixel);
                        }
                    }
                }
            }
        });


        device.color(new Image()
        {
            public void data(ByteBuffer data)
            {
                for(int y=0; y<height; y++)
                {
                    for(int x=0; x<width; x++)
                    {
                        int offset = 3*(y*width+x);

                        int r = data.get( offset+2 ) & 0xFF;
                        int g = data.get( offset+1 ) & 0xFF;
                        int b = data.get( offset+0 ) & 0xFF;

                        int pixel = (0xFF) << 24
                                  | (b & 0xFF) << 16
                                  | (g & 0xFF) << 8
                                  | (r & 0xFF) << 0;
                        color.setRGB(x, y, pixel);
                    }
                }
            }
        });

        device.acceleration(new Acceleration()
        {
            public void direction(float x, float y, float z)
            {
                System.out.printf("Acceleration: %f %f %f\n", x ,y ,z);
            }
        });

        int i = 0, count = 100;
        while(i++ < count)
        {
            context.processEvents();
        }

        device.depth(null);
        device.color(null);

        device.tilt(0);
        device.light(LEDStatus.LED_RED);

        device.dispose();

        String home = System.getProperty("user.home");

        ImageIO.write(color, "jpg", new File(home + File.separator + "kinect.color.jpg"));
        ImageIO.write(depth, "jpg", new File(home + File.separator + "kinect.depth.jpg"));
    }

    static int depth2rgb(short depth)
    {
        int r,g,b;

        float v = depth / 2047f;
        v = (float) Math.pow(v, 3)* 6;
		v = v*6*256;
        
		int pval = Math.round(v);
		int lb = pval & 0xff;
		switch (pval>>8) {
			case 0:
				b = 255;
				g = 255-lb;
				r = 255-lb;
				break;
			case 1:
				b = 255;
				g = lb;
				r = 0;
				break;
			case 2:
				b = 255-lb;
				g = 255;
				r = 0;
				break;
			case 3:
				b = 0;
				g = 255;
				r = lb;
				break;
			case 4:
				b = 0;
				g = 255-lb;
				r = 255;
				break;
			case 5:
				b = 0;
				g = 0;
				r = 255-lb;
				break;
			default:
				r = 0;
				g = 0;
				b = 0;
				break;
		}

        int pixel = (0xFF) << 24
                  | (b & 0xFF) << 16
                  | (g & 0xFF) << 8
                  | (r & 0xFF) << 0;

        return pixel;
    }

    static int depth2intensity(short depth)
    {
        int d = Math.round((1 - (depth / 2047f)) * 255f);
        int pixel = (0xFF) << 24
                  | (d & 0xFF) << 16
                  | (d & 0xFF) << 8
                  | (d & 0xFF) << 0;

        return pixel;
    }

}