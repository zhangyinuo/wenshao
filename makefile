SUBDIRS = lib 3rdlib network network/client network/dispatcher
installdir = /home/vfs/
#curday = $(shell date '+%Y%m%d')
all:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	echo "Making all in $$list"; \
	(cd $$subdir && make); \
	done;

clean:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	echo "Making all in $$list"; \
	(cd $$subdir && make clean); \
	done;

install:
	rm -rf $(installdir)/*;
	mkdir $(installdir)/bin -p;
	mkdir $(installdir)/log -p;
	mkdir $(installdir)/conf -p;
	mkdir $(installdir)/data -p;
	mkdir $(installdir)/path/tmpdir -p;
	mkdir $(installdir)/path/outdir -p;
	mkdir $(installdir)/path/datadir -p;
	mkdir $(installdir)/path/workdir -p;
	mkdir $(installdir)/path/indir -p;
	mkdir $(installdir)/path/bkdir -p;
	mkdir $(installdir)/path/delfile -p;
	cd network; cp wenshao $(installdir)/bin; cp wenshao.conf $(installdir)/conf;
	cd network/client; cp *.so $(installdir)/bin
	cd network/dispatcher; cp *.so $(installdir)/bin
	cp script/*.sh $(installdir)/bin

install_cdc_m:
	rm -rf $(installdir)/*;
	mkdir $(installdir)/bin -p;
	mkdir $(installdir)/log -p;
	mkdir $(installdir)/conf -p;
	cd network; cp cdc_master $(installdir)/bin; cp cdc_master.conf $(installdir)/conf;
	cd network/cdc_http; cp *.so $(installdir)/bin;

install_cdc_r:
	rm -rf $(installdir)/*;
	mkdir $(installdir)/bin -p;
	mkdir $(installdir)/log -p;
	mkdir $(installdir)/conf -p;
	cd network; cp cdc_r $(installdir)/bin; cp cdc_r.conf $(installdir)/conf;
	cd network/cdc_so; cp *.so $(installdir)/bin;

