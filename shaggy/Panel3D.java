import java.awt.*;

public class Panel3D extends Panel
	{
	static final int	LOWER = 1;
	static final int	HIGHER = 0;
	static final int	RIDGE = 2;

	public Insets	outside, ridge, plane;
	int		state = 0;
	
	public Panel3D( )
		{
		setUp( HIGHER, null, null, null );
		}

	public Panel3D( int state )
		{
		setUp( state, null, null, null );
		this.state = state;
		}
	
	public Panel3D( int state, Insets outside )
		{
		setUp( state, outside, null, null );
		}
	
	public Panel3D( int state, Insets outside, Insets ridge )
		{
		setUp( state, outside, ridge, null );
		}
	
	public Panel3D( int state, Insets outside, Insets ridge, Insets plane )
		{
		setUp( state, outside, ridge, plane );
		}

	public void setUp( int state, Insets outside, Insets ridge, Insets plane )
		{
		this.state		= state;
		
		this.outside	= (outside == null)	? new Insets( 3, 3, 3, 3 ) : outside;
		this.ridge		= addInsets( this.outside, (ridge == null) ? new Insets( 3, 3, 3, 3 ) : ridge );
		this.plane		= addInsets( ( (state & RIDGE) == RIDGE) ? this.ridge: this.outside, (plane == null ) ? new Insets( 4, 4, 4, 4 ) : plane );
		}

	final public Insets addInsets( Insets i1, Insets i2 )
		{
		return new Insets( i1.top + i2.top, i1.left + i2.left, i1.bottom + i2.bottom, i1.right + i2.right );
		}

	public void paint( Graphics g )
		{
		super.paint( g );
		Dimension d = size();
		Color bg = getBackground();

		g.setColor(bg);
		g.draw3DRect( outside.left, outside.top, d.width - outside.right - outside.left, d.height - outside.top - outside.bottom, (state & LOWER) != LOWER );
		if( (state & RIDGE) == RIDGE )
			g.draw3DRect( ridge.left, ridge.top, d.width - ridge.right - ridge.left, d.height - ridge.top - ridge.bottom, (state & LOWER) == LOWER ); 

		}

	public Insets insets() {
		return plane;
		}

	public Insets getInsets() {
		return plane;
		}
	}
	

