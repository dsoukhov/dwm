void
lefttile(Monitor *m)
{
	unsigned int i, n, h, smh, mw, my, ty;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (n == 0)
		return;

	if (n > m->nmaster)
		mw = m->nmaster ? m->ww * m->mfact : 0;
	else
		mw = m->ww;
	for (i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nmaster) {
			h = (m->wh - my) / (MIN(n, m->nmaster) - i);
      resize(c, m->wx + m->ww - mw, m->wy + my, mw - (2*c->bw), h - (2*c->bw), 0);
			if (my + HEIGHT(c) < m->wh)
				my += HEIGHT(c);
		} else {
			smh = m->mh * m->smfact;
			if(!(nexttiled(c->next)))
				h = (m->wh - ty) / (n - i);
			else
				h = (m->wh - smh - ty) / (n - i);
			if(h < minwsz) {
				c->isfloating = True;
				XRaiseWindow(dpy, c->win);
				resize(c, m->mx + (m->mw / 2 - WIDTH(c) / 2), m->my + (m->mh / 2 - HEIGHT(c) / 2), m->ww - mw - (2*c->bw), h - (2*c->bw), False);
				ty -= HEIGHT(c);
			}
			else
        resize(c, m->wx, m->wy + ty, m->ww - mw - (2*c->bw), h - (2*c->bw), 0);
			if(!(nexttiled(c->next)))
				ty += HEIGHT(c) + smh;
			else
				ty += HEIGHT(c);
		}
}

