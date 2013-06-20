/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */
package tcptransfertest;
import java.io.DataInputStream;
import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
//import java.net.UnknownHostException;

/**
 *
 * @author IronFox
 */
public class Server extends Thread {
    private int mseconds;
    private ServerSocket socket;
    private byte[] bytes;
    
    public Server(int port, int bytesPerPackage) throws IOException
    {
        //this.mseconds = mseconds;
        bytes = new byte[bytesPerPackage];
        socket = new ServerSocket(port);
    }
    private void Receive(Socket s) throws IOException
    {
        int read = 0;
        while (read < bytes.length)
        {
            read += s.getInputStream().read(bytes,0,bytes.length - read);
        }
    }
    
    @Override
    public void run()
    {
        try
        {
            Socket s= socket.accept();
            if (s !=null)
            {
                
                DataInputStream read = new DataInputStream( s.getInputStream());
                mseconds = read.readInt();
                
                float error;// = 0;
                Receive(s);
                long last = System.nanoTime();
                for (int i = 0; i < 9; i++)
                {
                    Receive(s);
                    //long now = System.nanoTime();
                    //float delta = ((float)(now - last)) / 1000000.0f;
                    //last = now;
                    //error += delta - (float)(mseconds);
                }
                long now = System.nanoTime();
                float delta = ((float)(now - last)) / 1000000.0f;
                error = delta - (float)(9*mseconds);
                System.out.println("Total error: "+(error));
            }
            s.close();
            socket.close();
        }
        catch (Exception e)
        {
            System.err.println(e);
        }
    }
    
}
