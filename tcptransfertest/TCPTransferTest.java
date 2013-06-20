/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */
package tcptransfertest;

import java.io.IOException;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 *
 * @author IronFox
 */
public class TCPTransferTest {

    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) {
        int mseconds = 125;
        int bytes =1024*1024*20;
        int port = 10002;
        try
        {
            if (args.length > 0)
            {
                for (int i = 1; i < 300; i++)
                {
                    Client c = new Client(i,bytes,args[0],port);
                    c.start();
                    c.join();
                }
            
            }
            else
            {
                Server s = new Server(port,bytes);
                s.start();
                s.join();
            }
            /*
            Server s = new Server(port,bytes);
            Client c = new Client(mseconds,bytes,"localhost",port);
            s.start();
            c.start();
            s.join();
            c.join();
            */
            System.out.println("all done");
        }
        catch (Exception ex) {
            Logger.getLogger(TCPTransferTest.class.getName()).log(Level.SEVERE, null, ex);
        }
        
    }
}
