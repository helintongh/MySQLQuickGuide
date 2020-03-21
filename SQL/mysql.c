


#include <stdio.h>
#include <string.h>

#include <mysql.h>


#define KING_DB_SERVER_IP		"192.168.199.134"
#define KING_DB_SERVER_PORT		3306

#define KING_DB_USERNAME		"root"//"admin"
#define KING_DB_PASSWORD		"hlt12345"//"123456"

#define KING_DB_DEFAULTDB		"KING_DB"


#define SQL_INSERT_TBL_USER		"INSERT TBL_USER(U_NAME, U_GENDER) VALUES('King', 'man');" //执行的插入语句
#define SQL_SELECT_TBL_USER		"SELECT * FROM TBL_USER;"	//执行的查询语句

#define SQL_DELETE_TBL_USER		"CALL PROC_DELETE_USER('King')"//删除操作
#define SQL_INSERT_IMG_USER		"INSERT TBL_USER(U_NAME, U_GENDER, U_IMG) VALUES('King', 'man', ?);" //？是占位符通过stmt操作,语句作用是往表中插入数据

#define SQL_SELECT_IMG_USER		"SELECT U_IMG FROM TBL_USER WHERE U_NAME='King';"


#define FILE_IMAGE_LENGTH		(64*1024)
// C U R D --> 
// 
//封装查询语句，来做查询
int king_mysql_select(MYSQL *handle) { //sql语句需要发送到db server
	// 执行sql语句
	// mysql_real_query --> sql
	if (mysql_real_query(handle, SQL_SELECT_TBL_USER, strlen(SQL_SELECT_TBL_USER))) {
		printf("mysql_real_query : %s\n", mysql_error(handle));
		return -1;
	}
	
	//存储结果
	// store --> 
	MYSQL_RES *res = mysql_store_result(handle);
	if (res == NULL) {
		printf("mysql_store_result : %s\n", mysql_error(handle));
		return -2;
	}
	//判断结果多少行/列
	// rows / fields
	int rows = mysql_num_rows(res);
	printf("rows: %d\n", rows);//打印出来有多少行
	
	int fields = mysql_num_fields(res);//有多少列
	printf("fields: %d\n", fields);
	//抓取数据
	// fetch
	MYSQL_ROW row;//每一行的数据
	while ((row = mysql_fetch_row(res))) { //打印出数据库里的每一行

		int i = 0;
		for (i = 0;i < fields;i ++) {
			printf("%s\t", row[i]);
		}
		printf("\n");
		
	}

	mysql_free_result(res);

	return 0;
}

// read_image和write_image作用是读写文件写入磁盘
// filename : path + file name
// buffer : store image data
// 读取图片存储在buffer里
int read_image(char *filename, char *buffer) {

	if (filename == NULL || buffer == NULL) return -1;
	
	FILE *fp = fopen(filename, "rb"); //只读方式读取文件
	if (fp == NULL) {
		printf("fopen failed\n");
		return -2;
	}

	// file size
	fseek(fp, 0, SEEK_END);	//把文件指针置到文件末尾
	int length = ftell(fp); // file size 返回目前文件指针到文件头的偏移量
	fseek(fp, 0, SEEK_SET);

	int size = fread(buffer, 1, length, fp);//读取的数据放到buffer里面,每次读1byte读length次
	if (size != length) { //读出来的长度不对读取失败
		printf("fread failed: %d\n", size);
		return -3;
	}

	fclose(fp);

	return size;

}

// filename : 文件路径
// buffer : 写入buffer
// length : 数据长度是多少
// 把图片保存到磁盘里面
int write_image(char *filename, char *buffer, int length) {

	if (filename == NULL || buffer == NULL || length <= 0) return -1;

	FILE *fp = fopen(filename, "wb+"); //写的方式打开文件,+代表如果没有则创建
	if (fp == NULL) {
		printf("fopen failed\n");
		return -2;
	}

	int size = fwrite(buffer, 1, length, fp);//buffer里面的数据写入到fp里，写length次每次1byte
	if (size != length) {
		printf("fwrite failed: %d\n", size);
		return -3;
	}

	fclose(fp);

	return size;
}
// MYSQL连接， buffer， buffer长度
// 图片存储， 写入数据库
int mysql_write(MYSQL *handle, char *buffer, int length) {

	if (handle == NULL || buffer == NULL || length <= 0) return -1;

	MYSQL_STMT *stmt = mysql_stmt_init(handle);//开启一条MYSQL通道，类似于开辟了一块空间。MYSQL_STMT在官方定义为预处理语句
	int ret = mysql_stmt_prepare(stmt, SQL_INSERT_IMG_USER, strlen(SQL_INSERT_IMG_USER));//参数分别为:stmt,执行的语句，执行语句长度。
	if (ret) { //返回0代表执行成功
		printf("mysql_stmt_prepare : %s\n", mysql_error(handle));
		return -2;
	}
	//绑定stmt,MYSQL_BIND和MYSQL_STMT是绝配
	//该结构用于语句输入（发送给服务器的数据值）和输出（从服务器返回的结果值）
	//对于输入，它与mysql_stmt_bind_param()一起使用，用于将参数数据值绑定到缓冲区上，以供mysql_stmt_execute()使用。
	//对于输出，它与mysql_stmt_bind_result()一起使用，用于绑定结果缓冲区，以便用于with mysql_stmt_fetch()以获取行。
	MYSQL_BIND param = {0}; 
	param.buffer_type  = MYSQL_TYPE_LONG_BLOB; //在数据库中为BLOB/TEXT型
	param.buffer = NULL; //对于输入，这是指向存储语句参数数据值的缓冲的指针。对于输出，它是指向返回结果集列值的缓冲的指针。
	param.is_null = 0;
	param.length = NULL;

	ret = mysql_stmt_bind_param(stmt, &param);//stmt和param绑定到一起。param的值输入到stmt里面
	if (ret) {
		printf("mysql_stmt_bind_param : %s\n", mysql_error(handle));
		return -3;
	}

	ret = mysql_stmt_send_long_data(stmt, 0, buffer, length);//允许程序分段的发送到mysql服务器
	if (ret) { //如果是非0值则失败
		printf("mysql_stmt_send_long_data : %s\n", mysql_error(handle));
		return -4;
	}

	ret = mysql_stmt_execute(stmt);//执行与句柄stmt的预处理相关查询
	if (ret) {
		printf("mysql_stmt_execute : %s\n", mysql_error(handle));
		return -5;
	}

	ret = mysql_stmt_close(stmt);//关闭stmt
	if (ret) {
		printf("mysql_stmt_close : %s\n", mysql_error(handle));
		return -6;
	}
	

	return ret;
}
// 图片存储,数据库中读取出来
int mysql_read(MYSQL *handle, char *buffer, int length) {
	//防止数组越界
	if (handle == NULL || buffer == NULL || length <= 0) return -1;

	MYSQL_STMT *stmt = mysql_stmt_init(handle);
	int ret = mysql_stmt_prepare(stmt, SQL_SELECT_IMG_USER, strlen(SQL_SELECT_IMG_USER));
	if (ret) {
		printf("mysql_stmt_prepare : %s\n", mysql_error(handle));
		return -2;
	}

	
	MYSQL_BIND result = {0};
	
	result.buffer_type  = MYSQL_TYPE_LONG_BLOB;//数据类型
	unsigned long total_length = 0;
	result.length = &total_length;//数据长度

	ret = mysql_stmt_bind_result(stmt, &result);//绑定结果
	if (ret) {
		printf("mysql_stmt_bind_result : %s\n", mysql_error(handle));
		return -3;
	}

	ret = mysql_stmt_execute(stmt);//执行stmt句柄绑定的sql语句
	if (ret) {
		printf("mysql_stmt_execute : %s\n", mysql_error(handle));
		return -4;
	}

	ret = mysql_stmt_store_result(stmt);//返回执行语句的结果
	if (ret) {
		printf("mysql_stmt_store_result : %s\n", mysql_error(handle));
		return -5;
	}


	while (1) {

		ret = mysql_stmt_fetch(stmt);//获取一行数据
		if (ret != 0 && ret != MYSQL_DATA_TRUNCATED) break; // 出现错误或出现数据截短

		int start = 0;
		while (start < (int)total_length) { //当文件指针的头小于文件的总长度时才继续取
			result.buffer = buffer + start; //数据库buffer是读出的，然后存入到结果集也就是result的buffer里面
			result.buffer_length = 1; // 每次去多少字节的数据，一个一个取
			mysql_stmt_fetch_column(stmt, &result, 0, start); //从当前结果集获取1列结果
			start += result.buffer_length; //起始位置+1
		}
	}

	mysql_stmt_close(stmt);

	return total_length;

}

int main() {
	//初始化mysql连接
	MYSQL mysql;
	//初始化失败返回NULL
	if (NULL == mysql_init(&mysql)) {
		printf("mysql_init : %s\n", mysql_error(&mysql));
		return -1;
	}
	//连接mysql返回非0成功，参数1.连接2.mysqlip地址3.数据库用户的名字4.数据库密码5.连接的是哪一个db数据库6.连接的端口号7.unix的socket默认null8.client_flag默认0
	if (!mysql_real_connect(&mysql, KING_DB_SERVER_IP, KING_DB_USERNAME, KING_DB_PASSWORD, 
		KING_DB_DEFAULTDB, KING_DB_SERVER_PORT, NULL, 0)) {

		printf("mysql_real_connect : %s\n", mysql_error(&mysql));
		goto Exit;
	}
	//用获取的mysql连接去操作数据库
	// mysql --> insert 
	printf("case : mysql --> insert \n");
#if 1
	if (mysql_real_query(&mysql, SQL_INSERT_TBL_USER, strlen(SQL_INSERT_TBL_USER))) { //执行返回0为成功
		printf("mysql_real_query : %s\n", mysql_error(&mysql));
		goto Exit;
	}
#endif

	king_mysql_select(&mysql);

	// mysql --> delete 

	printf("case : mysql --> delete \n");
#if 1
	if (mysql_real_query(&mysql, SQL_DELETE_TBL_USER, strlen(SQL_DELETE_TBL_USER))) {
		printf("mysql_real_query : %s\n", mysql_error(&mysql));
		goto Exit;
	}
#endif
	
	king_mysql_select(&mysql);



	printf("case : mysql --> read image and write mysql\n");
	
	char buffer[FILE_IMAGE_LENGTH] = {0}; //定义一个buffer用来存储文件
	int length = read_image("0voice.jpg", buffer); // 把图片读取出来
	if (length < 0) goto Exit; //如果长度小于0，直接退出释放连接
	
	mysql_write(&mysql, buffer, length); //调用该函数往数据库里写入图片


	printf("case : mysql --> read mysql and write image\n");
	
	memset(buffer, 0, FILE_IMAGE_LENGTH);
	length = mysql_read(&mysql, buffer, FILE_IMAGE_LENGTH);//从数据库中读取出图片

	write_image("a.jpg", buffer, length);

Exit:
	mysql_close(&mysql);//关闭释放掉mysql

	return 0;

}




