import java.awt.*;

public class NamedPanel extends Panel {
	public static final int		LEFT=0, CENTER=1, RIGHT=2;
	public static final int		TOP=0, MIDDLE=1, BOTTOM=2;

	boolean		clean;

	int		x_tl, y_tl;
	int		x_tlm, y_tlm;
	int		x_trm, y_trm;
	int		x_tr, y_tr;
	int		x_br, y_br;
	int		x_bl, y_bl;

	String		name;
	int		h_alignment, v_alignment;

	int		name_width, name_height, name_a_height;
	Color		back, nw, se;
	Insets		border, pane;
	Insets		theInsets;
	Dimension	old_size;

	NamedPanel( String name ) {
		this( name, LEFT, MIDDLE );
		}

	NamedPanel( String name, int h_alignment ) {
		this( name, h_alignment, MIDDLE );
		}

	NamedPanel( String name, int h_alignment, int v_alignment ) {
		border = new Insets( 4, 4, 4, 4 );
		pane = new Insets( 4, 4, 4, 4 );

		theInsets = addInsets( border, pane );

		this.name = name;
		this.h_alignment = h_alignment;
		this.v_alignment = v_alignment;
		clean = false;
		}

	public void addNotify() {
		super.addNotify();
		fontBox();

		theInsets = addInsets( border, pane );
		theInsets.top += name_height;
		validate();
		}

	final public Insets addInsets( Insets i1, Insets i2 )
		{
		return new Insets( i1.top + i2.top, i1.left + i2.left, i1.bottom + i2.bottom, i1.right + i2.right );
		}

	public Insets insets() {
		return theInsets;
		}

	public Insets getInsets() {
		return theInsets;
		}

	public void setLabel( String name ) {
		this.name = name;
		clean = false;
		repaint();
		}

	public void align( int alignment ) {
		h_alignment = alignment;
		clean = false;
		}

	public void paint( Graphics g ) {
		super.paint( g );

		Dimension	tmp = size();
		
		if( (!clean) || ((tmp.width != old_size.width) || (tmp.height != old_size.height) )  ) {
			old_size = tmp;

			calculateCrap();
			clean = true;
//			g.clearRect( 0, 0, tmp.width, tmp.height);
			}

		g.setColor( nw );
		g.drawLine( x_tl, y_tl, x_tlm, y_tlm );
		g.drawLine( x_trm, y_trm, x_tr, y_tr );
		g.drawLine( x_bl, y_bl, x_tl, y_tl );

		g.drawLine( x_tr-1, y_tr+1, x_br-1, y_br-1 );
		g.drawLine( x_bl+1, y_bl-1, x_br-1, y_br-1 );

		g.drawLine( x_trm, y_trm, x_trm, y_trm+1 );

		g.setColor( se );
		g.drawLine( x_tl+1, y_tl+1, x_tlm-1, y_tlm+1 );
		g.drawLine( x_trm+1, y_trm+1, x_tr-1, y_tr+1 );
		g.drawLine( x_bl+1, y_bl-1, x_tl+1, y_tl+1 );

		g.drawLine( x_tr, y_tr+1, x_br, y_br );
		g.drawLine( x_bl+1, y_bl, x_br, y_br );

		g.drawLine( x_tlm, y_tlm, x_tlm, y_tlm+1 );

		g.setColor( getForeground() );
		g.drawString( name, x_tlm + 5, border.top+name_a_height );
		}

	void fontBox() {
		FontMetrics	fm = getFontMetrics( getFont() );

		name_height = fm.getMaxAscent() + fm.getMaxDescent();
		name_a_height = fm.getMaxAscent();
		name_width = fm.stringWidth( name );
		}

	void calculateCrap() {
		fontBox();

		back = getBackground();
		se = back.brighter();
		nw = back.darker();

		x_tl = border.left;
		y_tl = border.top;

		switch( v_alignment ) {
			default: break;
			case MIDDLE: y_tl += name_height/2; break;
			case BOTTOM: y_tl += name_height; break;
			}

		x_tr = old_size.width - border.left;
		y_tr = y_tl;

		x_br = x_tr;
		y_br = old_size.height - border.bottom;

		x_bl = x_tl;
		y_bl = y_br;

		y_tlm = y_trm = y_tl;

		switch( h_alignment ) {
			default:
				x_tlm = x_tl + 20;
				x_trm = x_tlm + name_width + 8;
				break;
			case CENTER:
				x_tlm = (x_tl + (x_tr - x_tl)/2) - name_width/2 - 5;
				x_trm = x_tlm + name_width + 8;
				break;
			case RIGHT:
				x_trm = x_tr - 5;
				x_tlm = x_trm - (name_width + 8);
				break;
			}
		}
	}
