public class Queue {
	QElement	head, tail;

	Queue() {
		head = tail = null;
		}

	public synchronized void append( Object o ) {
		if( tail == null )
			head = tail = new QElement( o );
		else {
			tail.next = new QElement( o );
			tail = tail.next;
			}
		notifyAll();
		}

	public synchronized Object get() {
		try {
			while( head == null )
				wait();
			} catch( InterruptedException ie ) {
				return null;
			}

		Object		o = head.theData;
		head = head.next;

		if( head == null )
			tail = null;
		return o;
		}
	}

class QElement {
	QElement	next;
	Object		theData;

	QElement( Object o ) {
		next = null;
		theData = o;
		}
	}

