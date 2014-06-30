/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */
package distance.vector.routing;

import java.io.File;
import java.text.DecimalFormat;
import java.util.Arrays;
import java.util.Random;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerConfigurationException;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;
import org.w3c.dom.Attr;
import org.w3c.dom.Document;
import org.w3c.dom.Element;

/**
 *
 * @author IronFox
 */
public class DistanceVectorRouting {

    /**
     * Network node class, used to store distance matrices per node
     */
    public static class Node
    {
        private int[][] mat;
        public static int InvalidCost = Integer.MAX_VALUE;//!< Code used to indicate an invalid / empty matrix cell
        
        /**
         * Constructs a node for a given total number of nodes
         * @param numNodes Number of nodes (including self)
         */
        public Node(int numNodes)
        {
            mat = new int[numNodes][numNodes];
            for (int x = 0; x < mat.length; x++)
                for (int y = 0; y < mat[x].length; y++)
                    mat[x][y] = InvalidCost;
        }
        
        public int set(int to, int via, int cost)
        {
            mat[to][via] = cost;
            return cost;
        }
        /**
         * Searches for the out-bound node with the best routing costs to a given destination
         * @param to Index of the node to reach
         * @return Index of the out-bound node to route along
         */
        public int getBestRoute(int to)
        {
            int rs = 0;
            int cost = mat[to][0];
            for (int i = 1; i < mat[to].length; i++)
                if (mat[to][i] < cost)
                {
                    cost = mat[to][i];
                    rs = i;
                }
            return rs;
        }
        /**
         * Searches for the out-bound node with the best routing costs to a given destination, and returns that cost
         * @param to Index of the node to reach
         * @return  Cost to reach that node
         */
        public int getBestCost(int to)
        {
            return mat[to][getBestRoute(to)];
        }

        @Override
        public boolean equals(Object obj)
        {
            if (this==obj) {
                return true;
            }
            if (obj==null) {
                return false;
            }
            if (!(obj instanceof Node )) {
                return false; // different class
            }
            Node other = (Node) obj;
            for (int x = 0; x < mat.length; x++)
                for (int y = 0; y < mat[x].length; y++)
                    if (mat[x][y] != other.mat[x][y])
                        return false;
            return true;
            
        }

        static String intToString(int num, int digits) {
            assert digits > 0 : "Invalid number of digits";

            // create variable length array of zeros
            char[] zeros = new char[digits];
            Arrays.fill(zeros, '0');
            // format number as String
            DecimalFormat df = new DecimalFormat(String.valueOf(zeros));

            return df.format(num);
        }        
        
        /**
         * Prints the local matrix to the console
         */
        public void print()
        {
            for (int to = 0; to < mat.length; to++)
            {
                int best = getBestRoute(to);
                boolean printSpace = true;
                for (int via = 0; via < mat[to].length; via++)
                {
                    int cost = mat[to][via];
                    if (cost != InvalidCost)
                    {
                        if (via == best)
                        {
                            System.out.print(" ["+intToString(cost, 2)+"] ");
                            printSpace = false;
                        }
                        else
                        {
                            if (printSpace)
                            {
                                System.out.print("  ");
                            }
                            System.out.print(intToString(cost, 2));
                            printSpace = true;
                        }
                    }
                    else
                    {
                        if (printSpace)
                        {
                            System.out.print("  ");
                        }
                        System.out.print("--");
                        printSpace = true;
                    }
                }
                System.out.println();
            }
            
            
        }
    }
    
    /**
     * Network abstraction class
     */
    public static class Network
    {
        private int[][] matrix;
        private Node[] nodes;
        
        /**
         * Creates a network for a given, fixed number of nodes
         * @param numNodes Number of nodes supported by the local network
         */
        public Network(int numNodes)
        {
            matrix = new int[numNodes][numNodes];
            for (int x = 0; x < matrix.length; x++)
                for (int y = 0; y < matrix[x].length; y++)
                    matrix[x][y] = 0;
            resetNodeStates();
        }
        /**
         * Resets the local network node state
         */
        public final void resetNodeStates()
        {
            nodes = new Node[matrix.length];
            for (int i = 0; i < nodes.length; i++)
                nodes[i] = new Node(nodes.length);

        }
        
        /**
         * Progresses the local network matrices to optimize its routing tables
         * @return True, if something changed, false otherwise
         */
        public boolean iterate()
        {
            Node[] newState = new Node[nodes.length];   //create new node field
            for (int i = 0; i < nodes.length; i++)
            {
                Node n = newState[i] = new Node(nodes.length);  //create new node matrices
                for (int j = 0; j < nodes.length; j++) {
                    if (connected(i,j))                         //only processes connected neighbors of node i
                    {
                        int add = n.set(j,j,getWeight(i,j));    //get costs of the connection and initialize diagonal values in node matrix
                        Node other = nodes[j];                  //fetch previous matrix state of node j
                        for (int k = 0; k < nodes.length; k++)
                            if (k != i)                         //we don't record costs from node i to self, they don't make sense
                            {
                                int c = other.getBestCost(k);   //only copy best costs to node k
                                if (c != Node.InvalidCost)      //node j might not have costs to node k
                                    n.set(k,j,c+add);           //otherwise set to costs from node j to node k plus costs from node i to node j
                            }
                    }
                }
            }
            //detect if something changed:
            boolean complete = true;
            for (int i = 0; i < nodes.length; i++)
                if (!newState[i].equals(nodes[i]))
                {
                    complete = false;
                    break;
                }
            nodes = newState;   //copy over new state
            
            return !complete;
        }
        
        /**
         * Connects two nodes
         * The function returns if the connection already exists or connects to self
         * 
         * @param start Node to connect from (order does not matter)
         * @param end Node to connect to (order does not matter)
         * @param weight Weight to apply to the specified connection
         * @return True, if the connection is new and not self, false otherwise
         */
        public boolean connect(int start, int end, int weight)
        {
            if (start == end || matrix[start][end] != 0)
                return false;
            matrix[start][end] = matrix[end][start] = weight;
            return true;
        }
        
        /**
         * Disconnects two nodes
         * @param start First node to disconnect (order does not matter)
         * @param end First node to disconnect (order does not matter)
         * @return True, if a connection was found and removed, false otherwise
         */
        public boolean disconnect(int start, int end)
        {
            if (start == end || matrix[start][end] == 0)
                return false;
            matrix[start][end] = matrix[end][start] = 0;
            return true;
        }
        
        /**
         * Checks if two nodes are connected
         * @param start First node to check (order does not matter)
         * @param end Second node to check (order does not matter)
         * @return True, if the two nodes are connected, false otherwise
         */
        public boolean connected(int start, int end)
        {
            return matrix[start][end] != 0;
        }
        /**
         * Retrieves the weight of a connection
         * @param start First node to check (order does not matter)
         * @param end Second node to check (order does not matter)
         * @return Weight of a connection, 0 if the two nodes are not connected
         */
        public int getWeight(int start, int end)
        {
            return matrix[start][end];
        }
        
        /**
         * Queries the number of nodes in the local network
         * @return Number of nodes
         */
        public int countNodes() {return nodes.length;}

        /**
         * Iterates until the system balances out, and no further change could
         * be detected.
         * @param verbose Set true to display state to the console
         * @return Number of iterations needed to balance the system. At least 1.
         */
        public int solve(boolean verbose)
        {
            int iterationCounter = 0;
            while (iterate())
            {
                iterationCounter++;
                if (verbose)
                {
                    System.out.println("solver iteration "+iterationCounter +" ---------------------------------");
                    for (int i = 0; i < nodes.length; i++)
                    {
                        System.out.println("Node "+i+" state");
                        nodes[i].print();
                    }
                }
            }
            return iterationCounter;
        }
    };
    
    

    static Network n;
    
    static void connect(int from, int to, int weight)
    {
        n.connect(from, to, weight);
    
    }
    
    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) throws ParserConfigurationException, TransformerConfigurationException, TransformerException {
        
        
      
        if (false)
        {
            //sheet:
            n = new Network(4);
            n.connect(0,1,1);
            n.connect(0,2,4);
            n.connect(1,2,6);
            n.connect(1,3,2);
            n.connect(2,3,1);
        }
        
        if (false)
        {
            n = new Network(10);
            Random rnd = new Random(438593864);
            int cnt = 0;
            while (cnt < 25)//25)
            {
                if (n.connect(rnd.nextInt(n.countNodes()),rnd.nextInt(n.countNodes()),rnd.nextInt(9)+1))
                    cnt++;
            }
        }

        
        if (true)
        {
            n = new Network(10);
            connect(0, 1, 4);
            connect(0, 3, 5);
            connect(0, 5, 7);
            connect(0, 6, 1);
            connect(1, 2, 6);
            connect(1, 3, 7);
            connect(1, 4, 9);
            connect(1, 5, 2);
            connect(1, 6, 9);
            connect(2, 3, 4);
            connect(2, 5, 8);
            connect(2, 6, 5);
            connect(2, 8, 7);
            connect(3, 8, 6);
            connect(3, 9, 5);
            connect(4, 5, 6);
            connect(4, 6, 5);
            connect(4, 8, 9);
            connect(4, 9, 8);
            connect(5, 6, 3);
            connect(5, 8, 8);
            connect(5, 9, 2);
            connect(6, 9, 1);
            connect(7, 8, 4);
            connect(8, 9, 8);            
            
        
        }
        
        
        n.solve(true);
        n.disconnect(7, 8);
        n.resetNodeStates();
        n.solve(true);
    }
}
 