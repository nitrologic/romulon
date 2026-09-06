// mos6502.ts / assembler.ts
// (c)2026 nitrologic
// all rights reserved

// parse wozmon.s beneater roms
// parse commodoreplus.txt - Mike Dailly's ROM dissasembly of C16 / Plus4

import mos6502 from "mos6502.json" with { type: "json" };

let currentAddress=0;
const ram=new Uint8Array(65536);

const instructionSet=mos6502.instruction;
const modeProfile=mos6502.profile;

function lookOp(codes:string,index:number):string{
	return codes.slice(index*3,index*3+2);
}

function hex(i:number,count:number=2){
	i&=0xffff;
	return ""+i.toString(16).padStart(count,"0").toUpperCase();
}

console.log("nitrologic assembler 0.2.3");
console.log(mos6502.name,mos6502.version,mos6502.copyright);

function parseAscii(args:string):string[]{
	while(true){
		const q=args.indexOf("\"");if(q==-1) break;
		const qq=args.indexOf("\"",q+1);if(qq==-1) {
			console.log("parseBytes failure");
			break;
		}
		const text=args.slice(q+1,qq);
		// text to charcodes
		const charcodes=[];
		for(let i=0;i<text.length;i++){
			charcodes.push(text.charCodeAt(i));
		}
		args=args.slice(0,q)+charcodes.join(",")+args.slice(qq+1);
	}
	return args.split(",");	
}

// # labels

const labelMap=new Map<string,number>();

interface Labels {[address: number]: string;}

const labels:Labels={};

function setLabel(label:string,address:number):void{
	labels[address]=label;
	labelMap.set(label,address);
}

function findLabel(label:string,autoCreate=false):string{
	if(label.length){
		if(labelMap.has(label)) return label;
		if(autoCreate){
			labelMap.set(label,0);
			return label;
		}
	}
	return "";
}

function logLabels(): void {
	console.log("labels:{")
	const addresses: number[] = Object.keys(labels).map(Number).sort((a, b) => a - b);
	for (const address of addresses) {
		const label=labels[address];
		const hexAddress: string = address.toString(16).toUpperCase().padStart(4, '0');
		console.log("\t\""+hexAddress+"\":\""+label+"\",");
	}
	console.log("}")
}
// # comments

const comments:Record<number,string[]>={};

function addComment(comment:string,address:number):void{
	if(!Object.hasOwn(comments,address)){
		comments[address]=[];
	}
	comments[address].push(comment);
//	console.log("comment:",{address,comment});
}

function logComments(){
	console.log("comments={");
	console.log(comments);
	console.log("}");
}

// # equates

interface Equate {[address: number]: string;}

const equates:Record<number,string>={};
const equateByteMap=new Map<string,number>();
const equateWordMap=new Map<string,number>();

function addEquate(name:string,address:number):void{
	equates[address]=name;
	if(address<256){
		equateByteMap.set(name,address);
	}else{
		equateWordMap.set(name,address);
	}
}

function logWideEquates(): void {
	console.log("equates={");
	const map=equateWordMap;
	for(const name of map.keys()){
		const address=map.get(name)|0;
		console.log("\t\""+hex(address,4)+"\":\""+name+"\",");
	}
	console.log("}");
	return;
}

function logEquates(): void {
	console.log("equates={");
	const map=equateByteMap;
	for(const name of map.keys()){
		const address=map.get(name)|0;
//		const pad=(address<256)?2:4;
//		const hexAddress: string = address.toString(16).toUpperCase().padStart(pad, '0');
		console.log("\t\""+name+"\":\""+address+"\",");
	}
	console.log("}");
	return;
}

function byteReference(label:string):number{
	let address:number=labelMap.get(label)||-1;
	if(address==-1){
		if(equateByteMap.has(label)){
			address=equateByteMap.get(label)||-1;
		}
	}
	return address;
}

function addressReference(label:string):number{
	let address:number=labelMap.get(label)||-1;
	return address;
}

function hex8(name,lineNumber:number):string{
	if(name.startsWith("$")){
		const byte=parseInt(name.slice(1),16);
		return hex(byte);
	}
	if(equateByteMap.has(name)){
		const byte=equateByteMap.get(name)||-1;
		return hex(byte);
	}
	console.log("[HEX8] bad name",{name,lineNumber});
}

function hexChars(chars:string[]):string{
	const bytes=[];
	for(const char of chars){
		bytes.push(hex(parseInt(char)));
	}
	return bytes.join(" ");
}

function hex16(label:string,lineNumber:number):string{
	if(label.startsWith("$")){
		const word=parseInt(label.slice(1),16);
		return hex(word&0xff)+" "+hex((word>>8)&0xff);
	}
	let address:number=labelMap.get(label)||-1;
	console.log("[HEX16]",{label,address});
	if(address==-1){
		if(equateByteMap.has(label)){
			address=equateByteMap.get(label)||-1;
		}
	}
	if(lineNumber&&address==-1){
		console.log("bad address",address,label,lineNumber);
	}
	const result=[];
	result.push(hex(address&0xFF));
	result.push(hex((address>>8)&0xFF));
	return result.join(" ");
}

//interface Bits{bits:number,ref8?:number,ref16?:number,ref?:string};

function bits16(bits:Bits,lineNumber:number):string{
	if(bits.ref) return hex16(bits.ref,lineNumber);
	const word=bits.ref16||-1;
	return hex(word&0xff)+" "+hex(word>>8);
}

//	"ADC":{"opcodes":"69 65 75 6D 7D 79 61 71","flags":"NZCV","label":"ADD MEMORY TO ACCUMULATOR WITH CARRY","spec":15},

enum Mode{
	Brk,
	Immediate,
	ZeroPage,
	ZeroPageX,
	ZeroPageY,
	Absolute,
	AbsoluteX,
	AbsoluteY,
	IndirectX,
	IndirectY,
	None,	 // 10
	Branch,  // 11
	Error    // 12
};

interface Bits{bits:number,ref8?:number,ref16?:number,ref?:string};

interface MachineCode{mode:Mode,opcode:string,value?:string}

function machineHex(code:MachineCode):string{
	if(code.opcode){
		if(code.value){
			return code.opcode+" "+code.value;
		}else{
			return code.opcode;
		}
	}
	return "";
}

function decode6502(mnemonic:string,operand:string,line:string):MachineCode{
	const instruction=instructionSet[mnemonic]||null;
//	console.log("decode6502",{mnemonic,operand,instruction});
	const opcodes=instruction.opcodes;
	const spec=instruction.spec;
	if(operand=="") {
		const opcode=lookOp(opcodes,0);
		return {mode:Mode.None,opcode};
	}
	if(spec==6||spec==8){//JSR JMP
		const bits=decodeJump(operand);
		const opcode=lookOp(opcodes,0);
		return {mode:Mode.Absolute,opcode,value:bits16(bits)};
	}
	if(spec==5){
//		console.log("BRANCH",operand,line)
		const bits=decodeBranch(operand);
		const opcode=lookOp(opcodes,0);
		return {mode:Mode.Branch,opcode,value:hex(bits.ref8)};
	}
	const op=operand.toUpperCase();
	if (op.startsWith("#")) {
		const bits=decodeBits(operand.slice(1));
		const opcode=lookOp(opcodes,0);
//		console.log("[ASM] decode6502 immediate",{opcode,operand,op});
		return {mode:Mode.Immediate,opcode,value:hex(bits.ref8)};
	}
	if (op.startsWith("(")){
		if(op.endsWith("),Y")) {
			const index=op.indexOf(")");
			const bits=decodeBits(operand.slice(1,index));
			if(bits.bits==8){
				const opcode=lookOp(opcodes,2);
				return {mode:Mode.ZeroPageY,opcode,value:hex(bits.ref8)};
			}else{
				const opcode=lookOp(opcodes,7);
				return {mode:Mode.IndirectY,opcode,value:bits16(bits)};
			}
		}
		if (op.endsWith("),X")) {
			const index=op.indexOf(")");
			const bits=decodeBits(operand.slice(1,index));
			if(bits.bits==8){
				const opcode=lookOp(opcodes,2);
				return {mode:Mode.ZeroPageX,opcode,value:hex(bits.ref8)};
			}else{				
				const opcode=lookOp(opcodes,4);
				return {mode:Mode.AbsoluteX,opcode,value:bits16(bits)};
			}
		}
		if (op.endsWith(",X)")) {
			const index=op.indexOf(",");
			const bits=decodeBits(operand.slice(1,index));
			if(bits.bits==8){
				const opcode=lookOp(opcodes,2);
				return {mode:Mode.ZeroPageX,opcode,value:hex(bits.ref8),sticky:3};
			}else{
				const opcode=lookOp(opcodes,6);
				return {mode:Mode.IndirectX,opcode,value:bits16(bits),sticky:4};
			}
		}
		if (op.endsWith(")")) {
			const index=op.indexOf(")");
			const bits=decodeBits(operand.slice(1,index));
			return (bits.bits==8)?{mode:Mode.ZeroPage}:{mode:Mode.Absolute};
		}
		console.log("OH NO OH NO");
	}else{
		if (op.endsWith(",X")) {
			const index=op.indexOf(",");
			const bits=decodeBits(operand.slice(0,index));
			if(bits.bits==8){
				const opcode=lookOp(opcodes,2);
				return {mode:Mode.ZeroPageX,opcode,value:hex(bits.ref8)};
			}else{
				const opcode=lookOp(opcodes,4);
//				console.log("[,X]",opcode,bits)
				return {mode:Mode.AbsoluteX,opcode,value:bits16(bits)};
			}
		}
		if (op.endsWith(",Y")) {
			const index=op.indexOf(",");
			const bits=decodeBits(operand.slice(0,index));
			if(bits.bits==8){
				const opcode=lookOp(opcodes,2);
				return {mode:Mode.ZeroPageY,opcode,value:hex(bits.ref8)};
			}else{
				const opcode=lookOp(opcodes,5);
				return {mode:Mode.AbsoluteY,opcode,value:bits16(bits)};
			}
		}
		const bits=decodeBits(op);
		if(bits.bits==8){
			const opcode=lookOp(opcodes,1);
			return {mode:Mode.ZeroPage,opcode,value:hex(bits.ref8)};
		}else{
			const opcode=lookOp(opcodes,2);
			return {mode:Mode.Absolute,opcode,value:bits16(bits)};
		}
	}
}

function wideValue(slice:string){
	if(slice[0]=="$"){
		if(slice.length==5){
			const val16=parseInt(slice.slice(1,5),16);
//			console.log("[WIDE]",{val16});
			return val16;
		}
	}
	console.log("[WIDE] fail",slice);
}

// immediatemode#xxxx
function byteValue(slice:string,lineNumber:number):number{
	if(slice=="0"){
		return 0;
	}
	if(slice[0]=="$"){
		const val8=parseInt(slice.slice(1,3),16);
		return val8;
	}
	if(slice[0]=="%"){
		const val8=parseInt(slice.slice(1,9),2);
		return val8;
	}
	if(slice[0]=="<"){
		const label=slice.slice(1)
		const val16=byteReference(label);
		return val16&255;
	}
	if(slice[0]=="'"){
		const end=slice.indexOf("'",1);
		const bit=slice.slice(1,end);
		let val8=bit.charCodeAt(0);
		const suffix=slice.slice(end+1);
		if(suffix){
			switch(suffix){
				case "+$80":
					val8+=0x80;
					break;
				case "-1":
					val8-=1;
					break;
				default:
					console.log("[BYTE] sad suffix",suffix);
			}
		}
		return val8;
	}
	if(slice[0]==">"){
		const label=slice.slice(1)
		const val16=byteReference(label);
		return val16>>8;
	}
	const split=slice.split("-");
//	const diff=indexOf("-");
	if(split.length>1){
		const val0=byteReference(split[0]);
		const val1=byteReference(split[1]);
		return val1-val0;
	}
	const val=byteReference(slice);
	if(lineNumber&&val==-1){
		console.log("[BYTE] ?!?",slice,lineNumber);
	}
	return val;
}

function decodeJump(operand):Bits{
	if(operand.startsWith("$")){
		const val16=parseInt(slice.slice(1,5),16);
		return {bits:16,ref16:val16};
	}
	const label=findLabel(operand,true);
//	console.log("[decodejump] label",label,labelMap[label]);
	return {bits:16,ref:label};
}

function decodeBranch(operand):Bits{
//	console.log("BRANCH",operand);	
	const label=findLabel(operand,false);
	const target=byteReference(label);
	if(target){
		const delta=(currentAddress+2-target)&255;
//		console.log("branch",{delta,address,target});
		return {bits:8,ref8:delta};
	}
	// todo: support hex
	const value=parseInt(operand);
	return {bits:8,ref8:value};
}

function decodeBits(slice:string):Bits{
	if(slice.startsWith("$")){
		if(slice.length==5){
			const val16=parseInt(slice.slice(1,5),16);
//			console.log({opcode,operands,slice,val16});
			return {bits:16,ref16:val16};
		}
		if(slice.length==3){
			const val8=parseInt(slice.slice(1,3),16);
//			console.log({opcode,operands,slice,val8});
			return {bits:8,ref8:val8};
		}
	}

	if(slice.startsWith("%")){
		if(slice.length==17){
			const val16=parseInt(slice.slice(1,17),2);
			return {bits:16,ref16:val16};
		}
		if(slice.length==9){
			const val8=parseInt(slice.slice(1,9),2);
			return {bits:8,ref8:val8};
		}
	}

	if(equateByteMap.has(slice)){
		const ref8=equateByteMap.get(slice);
		return {bits:8,ref8};
	}
	if(equateWordMap.has(slice)){
		const ref16=equateWordMap.get(slice);
		return {bits:16,ref16};
	}
	const label=findLabel(slice,true);
	if(label){
		return {bits:16,ref:label};
	}
	console.log("unknown slice ???",slice);//opcode,operands);
}

// interface MachineCode{mode:Mode,opcode:string,value?:string}

function assembleLine(mnemonic:string,operands:string,line:string,lineNumber:number):MachineCode{
	const instruction=instructionSet[mnemonic];
	if(!instruction){
		console.log("[6502C ERROR] instruction not found for",mnemonic);
		return {mode:Mode.Error,opcode:"",value:"bad opcode "+mnemonic};
	}
	const machineCode=decode6502(mnemonic,operands,line);
//	console.log("[ASM] assembleLine",{machineCode,mnemonic,operands,lineNumber});	
	return machineCode;
}

// state of assembly

let operandSet=new Set<string>();

function currentComment():string{
	if(Object.hasOwn(comments,currentAddress)) return "; "+comments[currentAddress].join("");
	return "";
}

//interface instruction {opcodes:string,flags:string,label:string,spec:number}

interface Assembly{address:number,opcode:string,code?:MachineCode};


function parse6502(mnemonic:string,operands:string,line:string,lineNumber:number):Assembly{
	const opcode=mnemonic.toUpperCase();
	if(opcode=="ORG"){
		const orgAddress=wideValue(operands);
		currentAddress=orgAddress||0;
		return {address:currentAddress,opcode};
	}
//	if(lineNumber){console.log("[6502]",line);}

	const address=currentAddress;
	const code=assembleLine(opcode,operands,line,lineNumber);
	return {address,opcode,code};
}

// hex

function hexValues(bytes:Array<number>){
	const result=[];
	for(const byte of bytes){
		result.push(hex(byte));
	}
	return result.join(" ");
}

// dumpHex

function dumpHex(ram:Uint8Array,orgStart:number,orgEnd:number){
	for(let a=orgStart;a<orgEnd;a+=32){
		let code=[];
		for(let i=0;i<32;i++){
			code.push(ram[a+i].toString(16).padStart(2,"0"));
		}
		let a4=a.toString(16).padStart(4,"0");
		console.log(" \""+a4+":"+code.join("")+"\",");
	}
}

// assembler

const LabelLength=12;
const EqOffset=12;

let bulkCount=0;
let dropCount=0;
let blankCount=0;

let offset=0;

function pokeBytes(hexBytes:string){
	const bytes=hexBytes.split(" ");
	const n=bytes.length;
	for(let i=0;i<n;i++){
		const byte=parseInt(bytes[i],16);
		ram[currentAddress+i]=byte;
//		console.log("[POKE]",hex(currentAddress,4),n,hexBytes,i,byte);
	}
}

//dumpHex(mem,0xc000,0xffff)

// assemble

function assemble6502(opcode:string,operands:string,comment:string,label:string,hexBytes:string):string{
	if(label.length) setLabel(label,currentAddress);
	const bytes:string[]=[];
	//const hexBytes=bytes.join(" ");//
	//const hexBytes="00 00 00";
	const pad=" ".repeat(23-label.length);
	const a4=hex(currentAddress,4);
	const cols=[label+pad,a4,hexBytes+"      ",opcode,operands,comment];
	if(opcode!=""){
		if(opcode=="equ") {
			addEquate(label,currentAddress);
		}else{
			if(operands) operandSet.add(operands);
		}
	}
	return cols.join("\t");
}

async function fixMarkdown(src:string,dest:string,mem:Uint8Array){
	const text=await Deno.readTextFile(src);
	const lines=text.split("\n");
	const n=lines.length;
	const result=[];
	for(let i=0;i<n;i++){
		let line=lines[i];
		const tabs=line.split("\t");
		const n=tabs.length;
		if(n>1){
			line="\t"+line;
			if(n<5) line="\t"+line;
//			console.log(line);
		}
		result.push(line);
	}
	await Deno.writeTextFile(dest,result.join("\n"));
}


async function assembleMarkdown(src:string,mem:Uint8Array){
	const text=await Deno.readTextFile(src);
	const lines=text.split("\n");
	const n=lines.length;
	for(let i=0;i<n;i++){
		const line=lines[i];
		let split=line.split("\t");
		if(split.length>1){
			// to
			if(split.length>4){
				const label=split.shift();
				if(label){
					const labelAddress=parseInt(split[0],16)
//					console.log("[ASM]",{label,labelAddress});
					setLabel(label,labelAddress);
				}
			}
			const address=parseInt(split[0],16);
			const bytes=split[1].split(" ");
			const byte0=parseInt(bytes[0],16);
			const isBytes=(isFinite(byte0)&&(bytes[0].length==2||bytes[3]==" "))
			if(address&&isBytes){
				ram[address]=byte0;
				for(let i=1;i<bytes.length;i++){
					ram[address+i]=parseInt(bytes[i],16);
				}
//				console.log(line,split,address,bytes);
			}
		}
	}
}

async function assembleSource(src:string){
	const text=await Deno.readTextFile(src);
	const lines=text.split("\n");
	const n=lines.length;
	for(let pass=0;pass<2;pass++){
		currentAddress=0;
		for(let i=0;i<n;i++){
			const text=lines[i].replaceAll("\t"," ").replaceAll("\r","");
			const lineNumber=(pass==1)?(i+1):0;
			for(let j=0;j<text.length;j++){
				const c=text.charCodeAt(j);
				if(c<32||c>255){	//Ø is 216
					console.log("!!!!!!!!!!!!",line,c);
					continue;
				}
			}
			const semi=text.indexOf(";");
			if(semi!=-1){
				const comment=text.slice(semi+1).trim();
				if(currentAddress>0&&comment.length&&pass==1) {
					addComment(comment,currentAddress);
				}
			}
			if(semi==0) continue;

			//		if(i>85) break;
			const eol=(semi>-1)?semi:text.length;		
			const line=text.slice(0,eol).trimEnd();
			if(line.length==0) continue;

			const equ=line.indexOf("=");
			if(equ>-1&&equ<eol){
				const bits=line.substring(0,eol).split("=");
				const name=bits[0].trim();
				const value=bits[1].trim();
				let val=-1;
				if(value.startsWith("%")){
					val=parseInt(value.slice(1),2);
				}else if(value.startsWith("$")){
					val=parseInt(value.slice(1),16);
				}
				if(val<0){
					console.log("parse fail in equ",bits[0].trim(),value,lineNumber);
				}
//				console.log(name,"=",value,val);
				addEquate(name,val);
				console.log("[EQU4]",line,name,val);
				continue;
			}

			const space=line.indexOf(" ");
			let pos=0;
			let label="";
			const len=(space<0)?line.length:space;
			if(len){
				label=line.substring(0,len);
				pos+=len;
				if(label.endsWith(":")) label=label.slice(0,-1);
				if(currentAddress>0){
					setLabel(label,currentAddress)
				}
			}
			while(line.charAt(pos)==" ") pos++;
			// label cleared onto next non white space

			if(pos<eol && line.charAt(pos)!=" "){
				let hexBytes="";
				if(line.charAt(pos)=="."){
//					console.log("[trace] .",{pass,lineNumber,line});
					const eoc=line.indexOf(" ",pos);
					const command=line.slice(pos,eoc);
					const args=line.slice(eoc).trim();
					switch(command.toLowerCase()){
						case ".asciiz":{
							const chars=parseAscii(args);
							chars.push("0");
							hexBytes=hexChars(chars);
//							console.log("[.asciiz]",{args,chars,bytes});
							break;
						}
						case ".byte":{
							hexBytes=hexChars(args.split(","));
							console.log("[.BYTE]",hexBytes);
							break;
						}
						case".word":
							hexBytes=hex16(args);
							console.log("[WORD]",hexBytes);
							break;
						case".org":{
							currentAddress=wideValue(args);
							console.log(".org",currentAddress);
							break;
						}
						case".en":
							break;
						default:
							console.log("??"+command);
					}
					pos+=n;
					pokeBytes(hexBytes);
				}else{
					const opcode=line.substring(pos,pos+3).toLowerCase();
					pos+=opcode.length;
					while(line.charAt(pos)==" ") pos++;
					const operands=line.substring(pos,eol).trim();
					const comment=line.substring(eol).trim();	
// parse
// # MachineCode {mode:Mode,opcode:string,value?:string}
// # Assembly {address:number,opcode:string,code?:MachineCode};
					const assembly=parse6502(opcode,operands,line,lineNumber);
					let bytes=0;
					if(assembly){
						if( assembly.code){
							hexBytes=machineHex(assembly.code);
							const asm=assemble6502(opcode,operands,comment,label,hexBytes);
							if(lineNumber) {
//								console.log("[trace] code",{opcode,operands,comment,pass,lineNumber,hexBytes});
								const comment=currentComment();
//								console.log("[ASM3]",asm,comment);
								pokeBytes(hexBytes);
							}
						}
					}
				}
				const count=((hexBytes.length+1)/3)|0;
				currentAddress+=count;
			}
		}
	}
}

//await assembler("research/commodoreplus.txt")
//await assembleMarkdown("commodore/cbm1541.md",mem);
//dumpHex(mem,0xc000,0xffff)
//await fixMarkdown("commodore/cbm1541.md","commodore/cbm1541tidy.md",mem);
//LAB_00	= $00			; USR() JMP instruction

await assembleSource("kitset/wozmon.s");

//dumpHex(ram,0xc000,currentAddress);
//await assembler("kitset/diagnostics.s");

//await assembler("commodore/vic20roms.txt");
logLabels();
logEquates();
logWideEquates();
//logComments();
