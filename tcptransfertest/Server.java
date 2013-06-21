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
    
    /**
     * Creates a new server zocket and initializes receiving
     * 
     * @param port Port to install the server on
     * @param bytesPerPackage Number of bytes to receive per package. Must be identical to the number of bytes sent per package.
     * @throws IOException
     */
    public Server(int port, int bytesPerPackage) throws IOException
    {
        //this.mseconds = mseconds;
        bytes = new byte[bytesPerPackage];
        socket = new ServerSocket(port);
    }
    
    private void Receive(Socket s) throws IOException
    {
        int read = 0;
        while (read < bytes.length) //keep reading until the entire package has been received
        {
            read += s.getInputStream().read(bytes,0,bytes.length - read); //overwrite beginning of package storage each time, no need to actually keep the data
        }
    }
    
    @Override
    public void run()
    {
        try
        {
            System.out.println("awaiting client");
            while (true)
            {
                Socket s= socket.accept();  //wait for client
                if (s !=null)
                {

                    DataInputStream read = new DataInputStream( s.getInputStream());
                    mseconds = read.readInt();//read package interval

                    float error;// = 0;
                    Receive(s); //wait until client sent first package
                    long last = System.nanoTime();  //sample time
                    for (int i = 0; i < 9; i++) //receive the next nine packages
                    {
                        Receive(s);
                        //long now = System.nanoTime();
                        //float delta = ((float)(now - last)) / 1000000.0f;
                        //last = now;
                        //error += delta - (float)(mseconds);
                    }
                    long now = System.nanoTime();   //sample final time
                    float delta = ((float)(now - last)) / 1000000.0f;   //calculate delta in seconds
                    error = delta - (float)(9*mseconds);    //calculate error
                    System.out.println(mseconds + "\t"+(error)); //display error
                }
                s.close();
            }
                //socket.close();
        }
        catch (Exception e)
        {
            System.err.println(e);
        }
    }
    
}
