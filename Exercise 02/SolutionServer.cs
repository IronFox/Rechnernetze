using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace TestServer
{
    class Server
    {

        public Server()
        {
        }
    }
    

    class Program
    {
        static TcpListener tcpListener;


        static List<TcpClient> activeClients = new List<TcpClient>();

        static void Dispatch(byte[] buffer, string str)
        {
            System.Console.WriteLine(str);

            int at = 0;
            buffer[at++] = (byte)str.Length;
            for (int i = 0; i < str.Length; i++)
                buffer[at++] = (byte)str[i];


            lock (activeClients)
            {
                List<TcpClient> drop = new List<TcpClient>();
                foreach (var cl in activeClients)
                {
                    try
                    {
                        cl.GetStream().Write(buffer, 0, at);
                    }
                    catch
                    {
                        drop.Add(cl);
                    }
                }
                foreach (TcpClient cl in drop)
                {
                    if (activeClients.Remove(cl))
                        Dispatch(buffer, "Client disconnected: " + cl.Client.RemoteEndPoint.ToString());
                }
            }
            //System.Diagnostics.Debug.WriteLine(str);
        }

        static void HandleClientComm(object client_)
        {
            TcpClient tcpClient = (TcpClient)client_;
            NetworkStream clientStream = tcpClient.GetStream();

            byte[] message = new byte[0x100], buffer = new byte[0x100];
            byte[] len = new byte[1];
            Dispatch(buffer, "Client connected: " + tcpClient.Client.RemoteEndPoint.ToString());
            string name = tcpClient.Client.RemoteEndPoint.ToString() + ": ";
            
            int bytesRead;
            for (; ; )
            {
                bytesRead = 0;
                try
                {
                    //blocks until a client sends a message
                    bytesRead = clientStream.Read(len, 0, 1);
                    if (bytesRead != 1)
                        break;
                    int at = 0;
                    int length = len[0];
                    bool brk = false;
                    while (at < length)
                    {
                        bytesRead = clientStream.Read(buffer, at, length - at);
                        if (bytesRead <= 0)
                        {
                            brk = true;
                            break;
                        }
                        at += bytesRead;
                    }
                    if (brk)
                        break;
                    if (length > 0)
                    {
                        char[] str = new char[length];
                        for (int i = 0; i < length; i++)
                            str[i] = (char)buffer[i];

                        if (str[0] == ':')
                            name = new string(str, 1, length - 1) + ": ";
                        else
                            Dispatch(buffer, name + new string(str));
                    }

                }
                catch
                {
                    break;
                }


            }


            bool notify = false;
            lock (activeClients)
                notify = activeClients.Remove(tcpClient);

            if (notify)
                Dispatch(buffer, "Client disconnected: " + tcpClient.Client.RemoteEndPoint.ToString());
        }

        static void Main(string[] args)
        {
            tcpListener = new TcpListener(IPAddress.Any, 12345);

            tcpListener.Start();

            for (;;)
            {
                //blocks until a client has connected to the server
                TcpClient client = tcpListener.AcceptTcpClient();

                //create a thread to handle communication 
                //with connected client
                Thread clientThread = new Thread(new ParameterizedThreadStart(HandleClientComm));
                clientThread.Start(client);
                lock (activeClients)
                    activeClients.Add(client);
            }
        }
    }
}
