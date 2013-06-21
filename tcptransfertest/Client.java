/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */
package tcptransfertest;

import java.io.BufferedWriter;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.Socket;
import java.net.UnknownHostException;

/**
 *
 * @author IronFox
 */
public class Client extends Thread{
    private Socket  socket; //client socket
    private byte[]  data;   //packet to send each time
    private int mseconds;   //
    //BufferedReader inFromUser = new BufferedReader( new InputStreamReader(System.in));   Socket clientSocket = new Socket("localhost", 6789);   DataOutputStream outToServer = new DataOutputStream(clientSocket.getOutputStream());   BufferedReader inFromServer = new BufferedReader(new InputStreamReader(clientSocket.getInputStream()));   sentence = inFromUser.readLine();   - See more at: http://systembash.com/content/a-simple-java-tcp-server-and-tcp-client/#sthash.qjuWhyCe.dpuf
    /**
     * Creates a new client and connects it to the specified server
     * 
     * @param mseconds milliseconds between each packet
     * @param bytesPerPackage packet size in bytes
     * @param host address of the server to connect to
     * @param port host port to connect to
     * @throws UnknownHostException
     * @throws IOException
     */
    public Client(int mseconds, int bytesPerPackage, String host, int port) throws UnknownHostException, IOException
    {
        data = new byte[bytesPerPackage];
        socket = new Socket(host,port);
        this.mseconds = mseconds;
        DataOutputStream write= new DataOutputStream(socket.getOutputStream());
        write.writeInt(mseconds);
    }
    
    @Override
    public void    run()
    {
        try
        {
            long started = System.nanoTime();
            for (int i = 0; i < 10; i++)
            {
                long t = System.currentTimeMillis();    //retrieve time before sending
                socket.getOutputStream().write(data);
                long needed = System.currentTimeMillis() - t;   //calculate time needed for sending
                if (mseconds > needed) {
                    Thread.sleep(mseconds - needed);    //sleep if sending took less than desired interval
                }
            }
            long now = System.nanoTime();
            System.out.println("time needed to send 10x"+(data.length)+" bytes: "+((float)(now - started)) / 1000000000.0f);    //debug output of total seconds needed (actual measurement is done server-side
            socket.close();
        }
        catch (Exception ex)
        {
            System.err.println(ex);
        }
    }
}
