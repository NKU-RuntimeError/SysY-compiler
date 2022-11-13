#include <vector>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include "IR.h"
#include "lib.h"

// int getint()
static void addGetintPrototype() {
    std::vector<llvm::Type *> argTypes;
    llvm::FunctionType *funcType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(IR::ctx.llvmCtx),
            argTypes,
            false
    );
    llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            "getint",
            IR::ctx.module
    );
}

// int getch()
static void addGetchPrototype() {
    std::vector<llvm::Type *> argTypes;
    llvm::FunctionType *funcType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(IR::ctx.llvmCtx),
            argTypes,
            false
    );
    llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            "getch",
            IR::ctx.module
    );
}

// int getarray(int a[])
static void addGetarrayPrototype() {
    std::vector<llvm::Type *> argTypes{
        llvm::Type::getInt32PtrTy(IR::ctx.llvmCtx)
    };
    llvm::FunctionType *funcType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(IR::ctx.llvmCtx),
            argTypes,
            false
    );
    llvm::Function *func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            "getarray",
            IR::ctx.module
    );
    func->getArg(0)->setName("a");
}

// float getfloat()
static void addGetfloatPrototype() {
    std::vector<llvm::Type *> argTypes;
    llvm::FunctionType *funcType = llvm::FunctionType::get(
            llvm::Type::getFloatTy(IR::ctx.llvmCtx),
            argTypes,
            false
    );
    llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            "getfloat",
            IR::ctx.module
    );
}

// int getfarray(float a[])
static void addGetfarrayPrototype() {
    std::vector<llvm::Type *> argTypes{
        llvm::Type::getFloatPtrTy(IR::ctx.llvmCtx)
    };
    llvm::FunctionType *funcType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(IR::ctx.llvmCtx),
            argTypes,
            false
    );
    llvm::Function *func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            "getfarray",
            IR::ctx.module
    );
    func->getArg(0)->setName("a");
}

// void putint(int a)
static void addPutintPrototype() {
    std::vector<llvm::Type *> argTypes{
        llvm::Type::getInt32Ty(IR::ctx.llvmCtx)
    };
    llvm::FunctionType *funcType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(IR::ctx.llvmCtx),
            argTypes,
            false
    );
    llvm::Function *func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            "putint",
            IR::ctx.module
    );
    func->getArg(0)->setName("a");
}

// void putch(int a)
static void addPutchPrototype() {
    std::vector<llvm::Type *> argTypes{
        llvm::Type::getInt32Ty(IR::ctx.llvmCtx)
    };
    llvm::FunctionType *funcType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(IR::ctx.llvmCtx),
            argTypes,
            false
    );
    llvm::Function *func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            "putch",
            IR::ctx.module
    );
    func->getArg(0)->setName("a");
}

// void putarray(int n, int a[])
static void addPutarrayPrototype() {
    std::vector<llvm::Type *> argTypes{
        llvm::Type::getInt32Ty(IR::ctx.llvmCtx),
        llvm::Type::getInt32PtrTy(IR::ctx.llvmCtx)
    };
    llvm::FunctionType *funcType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(IR::ctx.llvmCtx),
            argTypes,
            false
    );
    llvm::Function *func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            "putarray",
            IR::ctx.module
    );
    func->getArg(0)->setName("n");
    func->getArg(1)->setName("a");
}

// void putfloat(float a)
static void addPutfloatPrototype() {
    std::vector<llvm::Type *> argTypes{
        llvm::Type::getFloatTy(IR::ctx.llvmCtx)
    };
    llvm::FunctionType *funcType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(IR::ctx.llvmCtx),
            argTypes,
            false
    );
    llvm::Function *func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            "putfloat",
            IR::ctx.module
    );
    func->getArg(0)->setName("a");
}

// void putfarray(int n, float a[])
static void addPutfarrayPrototype() {
    std::vector<llvm::Type *> argTypes{
        llvm::Type::getInt32Ty(IR::ctx.llvmCtx),
        llvm::Type::getFloatPtrTy(IR::ctx.llvmCtx)
    };
    llvm::FunctionType *funcType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(IR::ctx.llvmCtx),
            argTypes,
            false
    );
    llvm::Function *func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            "putfarray",
            IR::ctx.module
    );
    func->getArg(0)->setName("n");
    func->getArg(1)->setName("a");
}

// void _sysy_starttime(int lineno)
static void addSysyStarttimePrototype() {
    std::vector<llvm::Type *> argTypes{
        llvm::Type::getInt32Ty(IR::ctx.llvmCtx)
    };
    llvm::FunctionType *funcType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(IR::ctx.llvmCtx),
            argTypes,
            false
    );
    llvm::Function *func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            "_sysy_starttime",
            IR::ctx.module
    );
    func->getArg(0)->setName("lineno");
}

// void _sysy_stoptime(int lineno)
static void addSysyStoptimePrototype() {
    std::vector<llvm::Type *> argTypes{
        llvm::Type::getInt32Ty(IR::ctx.llvmCtx)
    };
    llvm::FunctionType *funcType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(IR::ctx.llvmCtx),
            argTypes,
            false
    );
    llvm::Function *func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            "_sysy_stoptime",
            IR::ctx.module
    );
    func->getArg(0)->setName("lineno");
}

void addLibraryPrototype() {
    addGetintPrototype();
    addGetchPrototype();
    addGetarrayPrototype();
    addGetfloatPrototype();
    addGetfarrayPrototype();
    addPutintPrototype();
    addPutchPrototype();
    addPutarrayPrototype();
    addPutfloatPrototype();
    addPutfarrayPrototype();
    addSysyStarttimePrototype();
    addSysyStoptimePrototype();
}
