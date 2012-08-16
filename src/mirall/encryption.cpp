/*
 * Copyright (C) 2012 Bjoern Schiessle <schiessle@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "encryption.h"
#include <QMap>
#include <iostream>
#include <QtNetwork>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QAuthenticator>
#include <QDomDocument>

Encryption::Encryption(QString baseurl, QString username, QString password, QObject *parent) :
    QObject(parent)
{
    setBaseUrl(baseurl);
    setAuthCredentials(username, password);
    _nam = new QNetworkAccessManager(this);

    connect( _nam, SIGNAL(finished(QNetworkReply*)),this,SLOT(slotHttpRequestResults(QNetworkReply*)));
    connect( _nam, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)), this, SLOT(slotHttpAuth(QNetworkReply*,QAuthenticator*)));
}

void Encryption::setBaseUrl(QString baseurl)
{
    _baseurl = baseurl;
}

void Encryption::setAuthCredentials(QString username, QString password) {
    _username = username;
    _password = password;
}

QMap<QString, QString> Encryption::parseXML(QString xml, QList<QString> tags)
{
    QMap<QString, QString> result;

    QDomDocument doc("results");
    doc.setContent(xml);

    for (int i = 0; i < tags.size(); ++i) {
        QString tag = tags.at(i);
        QDomNodeList elements = doc.elementsByTagName(tag);
        if (elements.size() ==  1 ) {
            result[tag] = elements.at(0).toElement().text();
        }
    }
    return result;
}

void Encryption::getUserKeys()
{
    QNetworkReply *reply = _nam->get(QNetworkRequest(QUrl(_baseurl + "/ocs/v1.php/cloud/userkeys")));
    _directories[reply] = Encryption::GetUserKeys;
}

void Encryption::generateUserKeys(QString password)
{
    QString privatekey("foo-PRIVATE"); // dummy key for the moment
    QString publickey("foo-PUBLIC"); // dummy key for the moment

    QUrl postData;
    postData.addQueryItem("privatekey", privatekey);
    postData.addQueryItem("publickey", publickey);

    QNetworkRequest req;
    req.setUrl(QUrl(_baseurl + "/ocs/v1.php/cloud/userkeys"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QNetworkReply *reply = _nam->post(req, postData.encodedQuery());
    _directories[reply] = Encryption::SetUserKeys;

    QEventLoop eventLoop;
    // stay here until request is finished to avoid losing postData to early
    connect(_nam, SIGNAL(finished()), &eventLoop, SLOT(quit()));
    eventLoop.exec();
}

void Encryption::slotHttpRequestResults(QNetworkReply *reply)
{
    OCSCalls call = _directories.value(reply);
    QMap<QString, QString> result;
    QList<QString> returnValues;

    if(reply->error() == QNetworkReply::NoError) {

        QString xml =  reply->readAll();

        switch (call)
        {
        case Encryption::GetUserKeys:
            returnValues << "statuscode" << "publickey" << "privatekey";
            result = parseXML(xml, returnValues);
            emit ocsGetUserKeysResults(result);
            break;
        case Encryption::SetUserKeys:
            returnValues << "statuscode";
            result = parseXML(xml, returnValues);
            emit ocsSetUserKeysResults(result);
            break;
        default :
            qDebug() << "Something went wrong!";
        }

    } else {
        qDebug() << reply->errorString();
    }
    _directories.remove(reply);
}

void Encryption::slotHttpAuth(QNetworkReply *reply, QAuthenticator *auth)
{
    auth->setUser(_username);
    auth->setPassword(_password);
}