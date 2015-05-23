create table t_port (
	id int NOT NULL AUTO_INCREMENT,
	port mediumint(16) unsigned NOT NULL default '22',
	primary key (id)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

create table t_url(
	id int NOT NULL AUTO_INCREMENT,
	url varchar(1024) NOT NULL default 'xxxx',
	primary key (id)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

create table t_target(
	id int NOT NULL AUTO_INCREMENT,
	target varchar(256) NOT NULL default 'facebook.com',
	primary key (id)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

create table t_sub(
	id int NOT NULL AUTO_INCREMENT,
	sub varchar(256) NOT NULL default 'www',
	primary key (id)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

create table t_msg (
	domain	varchar(256)  not null;
	ip      varchar(16)   not null;
	port    int           not null;
	msg     varchar(2048) not null;
	primary key(domain, ip, port)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;


create table t_http_msg (
	domain	varchar(256)  not null;
	ip      varchar(16)   not null;
	port    int           not null;
	url     varchar(256)  not null;
	msg     varchar(2048) not null;
	retcode int           not null;
	primary key(domain, ip, port, url)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

