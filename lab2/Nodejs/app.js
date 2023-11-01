var express             = require('express');
var bodyParse           = require('body-parser')
var app                 = express();
app.use(bodyParse.urlencoded({extended:false})) ;
 app.use('/static', express.static('static'))

// 处理根目录的get请求
app.get('/',function(req,res){
    res.sendfile('main.html') ;
    console.log('服务器等待请求中...');
}) ;

// 监听3000端口
var server=app.listen(3000,function(){
    console.log('app is listening at http://localhost:3000/ \n 请手动打开网址');
}) ;