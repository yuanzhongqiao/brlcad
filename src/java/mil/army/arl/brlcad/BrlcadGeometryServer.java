/* Generated by Together */

package mil.army.arl.brlcad;

import mil.army.arl.muves.geometry.*;
import java.rmi.RemoteException;

public class BrlcadGeometryServer implements GeometryServer {
    public BrlcadGeometryServer() 
	throws java.rmi.RemoteException {
    
    }

    public boolean loadGeometry(String geomInfo) 
	throws RemoteException {
        return false;
    }

    public Vect shootRay(Point origin, Vect dir) 
	throws RemoteException {
        return null;
    }

    public BoundingBox getBoundingBox() 
	throws RemoteException {
        return null;
    }

    public BoundingBox getBoundingBox(String item) 
	throws RemoteException {
        return null;
    }

    public String getTitle() throws RemoteException {
        return null;
    }

    public boolean makeHole(Point origin, Vect dir, 
			    float baseDiam, float topDiam) 
	throws RemoteException {
        return false;
    }
}
